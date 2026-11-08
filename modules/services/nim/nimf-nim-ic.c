/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-nim-ic.c
 * This file is part of Nimf.
 *
 * Copyright (C) 2015-2019 Hodong Kim <cogniti@gmail.com>
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

#include "nimf-nim-ic.h"
#include <string.h>
#include "nimf-connection.h"
#include "nimf-message-private.h"

G_DEFINE_TYPE (NimfNimIC, nimf_nim_ic, NIMF_TYPE_SERVICE_IC);

/* Hold the transport across nested dispatch, including peer disconnects. */
static gboolean
nimf_nim_ic_request (NimfNimIC *nic, NimfMessageType type,
                     gpointer data, gsize size, GDestroyNotify destroy)
{
  g_autoptr(NimfConnection) connection = nic->connection ?
    g_object_ref (nic->connection) : NULL;
  if (connection == NULL || connection->result->cancelled)
  {
    if (destroy)
      destroy (data);
    return FALSE;
  }

  if (!nimf_send_message (connection->socket, nic->icid, type, data, size, destroy))
  {
    connection->result->cancelled = TRUE;
    return FALSE;
  }
  nimf_result_iteration_until (connection->result, NULL, nic->icid, type + 1);
  NimfMessage *reply = connection->result->reply;
  if (connection->result->cancelled || reply == NULL)
    return FALSE;
  if (reply->header.data_len == sizeof (gboolean))
  {
    gboolean value;
    memcpy (&value, reply->data, sizeof value);
    return value;
  }
  return TRUE;
}

void
nimf_nim_ic_emit_commit (NimfServiceIC *ic, const gchar *text)
{
  nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_COMMIT,
                       (gpointer) text, strlen (text) + 1, NULL);
}

void
nimf_nim_ic_emit_preedit_start (NimfServiceIC *ic)
{
  nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_PREEDIT_START, NULL, 0, NULL);
}

void
nimf_nim_ic_emit_preedit_changed (NimfServiceIC *ic,
                                  const gchar *preedit_string,
                                  NimfPreeditAttr **attrs, gint cursor_pos)
{
  gsize str_len = strlen (preedit_string);
  gsize n_attr = 0;
  while (attrs[n_attr])
    n_attr++;
  gsize size = str_len + 1 + n_attr * sizeof (NimfPreeditAttr) + sizeof (gint);
  gchar *data = g_malloc (size);
  memcpy (data, preedit_string, str_len + 1);
  for (gsize i = 0; i < n_attr; i++)
    memcpy (data + str_len + 1 + i * sizeof (NimfPreeditAttr),
            attrs[i], sizeof (NimfPreeditAttr));
  memcpy (data + size - sizeof cursor_pos, &cursor_pos, sizeof cursor_pos);
  nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_PREEDIT_CHANGED, data, size, g_free);
}

void
nimf_nim_ic_emit_preedit_end (NimfServiceIC *ic)
{
  nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_PREEDIT_END, NULL, 0, NULL);
}

gboolean
nimf_nim_ic_emit_retrieve_surrounding (NimfServiceIC *ic)
{
  return nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_RETRIEVE_SURROUNDING,
                              NULL, 0, NULL);
}

gboolean
nimf_nim_ic_emit_delete_surrounding (NimfServiceIC *ic, gint offset, gint n_chars)
{
  gint data[] = { offset, n_chars };
  return nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_DELETE_SURROUNDING,
                              data, sizeof data, NULL);
}

void
nimf_nim_ic_emit_beep (NimfServiceIC *ic)
{
  nimf_nim_ic_request (NIMF_NIM_IC (ic), NIMF_MESSAGE_BEEP, NULL, 0, NULL);
}

const gchar *
nimf_nim_ic_get_service_id (NimfServiceIC *ic)
{
  return "nimf-nim";
}

NimfNimIC *
nimf_nim_ic_new (guint16         icid,
                 NimfConnection *connection)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfNimIC *nic = g_object_new (NIMF_TYPE_NIM_IC, NULL);

  g_set_weak_pointer (&nic->connection, connection);
  nic->icid = icid;

  return nic;
}

static void
nimf_nim_ic_init (NimfNimIC *nic)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

static void
nimf_nim_ic_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_clear_weak_pointer (&NIMF_NIM_IC (object)->connection);
  G_OBJECT_CLASS (nimf_nim_ic_parent_class)->finalize (object);
}

static void
nimf_nim_ic_class_init (NimfNimICClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass       *object_class     = G_OBJECT_CLASS (class);
  NimfServiceICClass *service_im_class = NIMF_SERVICE_IC_CLASS (class);

  object_class->finalize = nimf_nim_ic_finalize;

  service_im_class->emit_commit          = nimf_nim_ic_emit_commit;
  service_im_class->emit_preedit_start   = nimf_nim_ic_emit_preedit_start;
  service_im_class->emit_preedit_changed = nimf_nim_ic_emit_preedit_changed;
  service_im_class->emit_preedit_end     = nimf_nim_ic_emit_preedit_end;
  service_im_class->emit_retrieve_surrounding = nimf_nim_ic_emit_retrieve_surrounding;
  service_im_class->emit_delete_surrounding = nimf_nim_ic_emit_delete_surrounding;
  service_im_class->emit_beep            = nimf_nim_ic_emit_beep;
  service_im_class->get_service_id       = nimf_nim_ic_get_service_id;
}
