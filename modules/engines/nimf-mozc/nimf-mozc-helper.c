/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc-helper.c
 * This file is part of Nimf.
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nimf-mozc-helper.h"
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <string.h>

#define NIMF_MOZC_READ_SIZE 4096
#define NIMF_MOZC_POLL_MS   50

struct _NimfMozcHelper
{
  gchar         *program;
  gint           timeout_ms;
  gint           retry_delay_ms;
  GSubprocess   *process;
  GOutputStream *input;
  GInputStream  *output;
  GString       *buffer;
  guint          session_id;
  guint          event_id;
  gint64         retry_time;
};

static void
helper_stop (NimfMozcHelper *helper)
{
  if (helper->process)
    g_subprocess_force_exit (helper->process);

  g_clear_object (&helper->process);
  helper->input  = NULL;
  helper->output = NULL;
  g_string_truncate (helper->buffer, 0);
}

/* A failure that costs time must not repeat on every key press. It can block
 * the server for timeout_ms each time. */
static void
helper_delay_retry (NimfMozcHelper *helper)
{
  helper->retry_time = g_get_monotonic_time () +
                       (gint64) helper->retry_delay_ms * G_TIME_SPAN_MILLISECOND;
}

/* The next key press restarts the helper at once. Japanese input stays
 * available, and an exit is cheap to detect. */
static void
helper_lost (NimfMozcHelper *helper, const gchar *reason)
{
  g_warning ("nimf-mozc: %s: %s", helper->program, reason);

  helper_stop (helper);
}

static void
helper_fail (NimfMozcHelper *helper, const gchar *reason)
{
  helper_lost (helper, reason);
  helper_delay_retry (helper);
}

static gchar *
buffer_take_line (GString *buffer)
{
  gchar *newline = memchr (buffer->str, '\n', buffer->len);
  gchar *line;

  if (newline == NULL)
    return NULL;

  line = g_strndup (buffer->str, newline - buffer->str);
  g_string_erase (buffer, 0, newline - buffer->str + 1);

  return line;
}

static gboolean
helper_poll (NimfMozcHelper *helper, gint timeout_ms)
{
  GPollFD poll_fd;

  poll_fd.fd      = g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (helper->output));
  poll_fd.events  = G_IO_IN | G_IO_HUP | G_IO_ERR;
  poll_fd.revents = 0;

  return g_poll (&poll_fd, 1, timeout_ms) > 0;
}

static gchar *
helper_read_line (NimfMozcHelper *helper)
{
  GPollableInputStream *stream = G_POLLABLE_INPUT_STREAM (helper->output);
  gint64 deadline = g_get_monotonic_time () +
                    (gint64) helper->timeout_ms * G_TIME_SPAN_MILLISECOND;
  gchar  chunk[NIMF_MOZC_READ_SIZE];

  while (TRUE)
  {
    gchar *line = buffer_take_line (helper->buffer);
    gint64 remaining_ms;
    gssize n_read;

    if (line)
      return line;

    remaining_ms = (deadline - g_get_monotonic_time ()) / G_TIME_SPAN_MILLISECOND;

    if (remaining_ms <= 0)
    {
      helper_fail (helper, "timed out waiting for a reply");
      return NULL;
    }

    /* mozc_server inherits this pipe, so a dead helper gives no EOF while the
     * server runs. Check the process itself between short polls. */
    if (!helper_poll (helper, MIN (remaining_ms, NIMF_MOZC_POLL_MS)))
    {
      if (g_subprocess_get_identifier (helper->process) == NULL)
      {
        helper_lost (helper, "the process exited");
        return NULL;
      }

      continue;
    }

    n_read = g_pollable_input_stream_read_nonblocking (stream, chunk,
                                                       sizeof (chunk),
                                                       NULL, NULL);
    if (n_read <= 0)
    {
      helper_lost (helper, "the process exited");
      return NULL;
    }

    g_string_append_len (helper->buffer, chunk, n_read);
  }
}

static NimfMozcSexp *
helper_read_reply (NimfMozcHelper *helper)
{
  gchar        *line  = helper_read_line (helper);
  NimfMozcSexp *reply;

  if (line == NULL)
    return NULL;

  reply = nimf_mozc_sexp_parse (line);
  g_free (line);

  if (reply == NULL)
    helper_fail (helper, "malformed reply");

  return reply;
}

static NimfMozcSexp *
helper_request (NimfMozcHelper *helper, const gchar *command)
{
  gchar        *line;
  gboolean      written;
  NimfMozcSexp *reply;

  helper->event_id++;
  line = g_strdup_printf ("(%u %s)\n", helper->event_id, command);
  written = g_output_stream_write_all (helper->input, line, strlen (line),
                                       NULL, NULL, NULL);
  g_free (line);

  if (!written)
  {
    helper_lost (helper, "the process closed its input");
    return NULL;
  }

  reply = helper_read_reply (helper);

  if (reply == NULL)
    return NULL;

  if (nimf_mozc_sexp_lookup (reply, "error") ||
      nimf_mozc_sexp_get_int (reply, "emacs-event-id", -1) != (gint) helper->event_id)
  {
    nimf_mozc_sexp_free (reply);
    helper_fail (helper, "error reply");
    return NULL;
  }

  return reply;
}

static gboolean
helper_read_greeting (NimfMozcHelper *helper)
{
  NimfMozcSexp *greeting = helper_read_reply (helper);
  gboolean      is_valid;

  if (greeting == NULL)
    return FALSE;

  is_valid = nimf_mozc_sexp_lookup (greeting, "mozc-emacs-helper") != NULL;
  nimf_mozc_sexp_free (greeting);

  if (!is_valid)
    helper_fail (helper, "unexpected greeting");

  return is_valid;
}

static gboolean
helper_create_session (NimfMozcHelper *helper)
{
  NimfMozcSexp *reply = helper_request (helper, "CreateSession");

  if (reply == NULL)
    return FALSE;

  helper->session_id = nimf_mozc_sexp_get_int (reply, "emacs-session-id", 0);
  nimf_mozc_sexp_free (reply);

  return TRUE;
}

static gboolean
helper_start (NimfMozcHelper *helper)
{
  GError *error = NULL;

  if (helper->process)
    return TRUE;

  if (g_get_monotonic_time () < helper->retry_time)
    return FALSE;

  /* mozc_server creates only the last path level of its profile directory.
   * Without this parent it exits silently and SendKey fails 40 seconds later. */
  g_mkdir_with_parents (g_get_user_config_dir (), 0700);

  helper->process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                      &error, helper->program, NULL);
  if (helper->process == NULL)
  {
    helper_fail (helper, error->message);
    g_error_free (error);
    return FALSE;
  }

  helper->input  = g_subprocess_get_stdin_pipe  (helper->process);
  helper->output = g_subprocess_get_stdout_pipe (helper->process);

  if (helper_read_greeting (helper) && helper_create_session (helper))
    return TRUE;

  helper_delay_retry (helper);

  return FALSE;
}

/**
 * nimf_mozc_helper_new:
 * @program: the helper executable, searched in PATH
 * @timeout_ms: how long one reply may take
 * @retry_delay_ms: how long to wait before restarting after a failure
 *
 * The process starts on the first nimf_mozc_helper_send_key() call.
 *
 * Returns: a new #NimfMozcHelper
 */
NimfMozcHelper *
nimf_mozc_helper_new (const gchar *program,
                      gint         timeout_ms,
                      gint         retry_delay_ms)
{
  NimfMozcHelper *helper = g_new0 (NimfMozcHelper, 1);

  /* A write to a dead helper must fail, not kill the nimf server. */
  signal (SIGPIPE, SIG_IGN);

  helper->program        = g_strdup (program);
  helper->timeout_ms     = timeout_ms;
  helper->retry_delay_ms = retry_delay_ms;
  helper->buffer         = g_string_new (NULL);

  return helper;
}

void
nimf_mozc_helper_free (NimfMozcHelper *helper)
{
  helper_stop (helper);
  g_string_free (helper->buffer, TRUE);
  g_free (helper->program);
  g_free (helper);
}

/**
 * nimf_mozc_helper_send_key:
 * @helper: a #NimfMozcHelper
 * @tokens: the SendKey arguments
 *
 * Returns: (nullable): the full reply, or %NULL when the helper is unavailable.
 *   mozc state is lost in that case, so the caller must drop its composition.
 */
NimfMozcSexp *
nimf_mozc_helper_send_key (NimfMozcHelper *helper, const gchar *tokens)
{
  gchar        *command;
  NimfMozcSexp *reply;

  if (!helper_start (helper))
    return NULL;

  command = g_strdup_printf ("SendKey %u %s", helper->session_id, tokens);
  reply   = helper_request (helper, command);
  g_free (command);

  return reply;
}
