/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * test-nimf-mozc.c
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

#include <glib.h>
#include <nimf.h>
#include "nimf-mozc-helper.h"
#include "nimf-mozc-key.h"
#include "nimf-mozc-protocol.h"
#include "nimf-mozc-sexp.h"

/* Recorded from mozc_emacs_helper 2.29.5160.102, shortened to the used fields. */
static const gchar conversion_reply[] =
  "((emacs-event-id . 58)(emacs-session-id . 1)(output . ("
  "(id . \"9606594367642925272\")(mode . hiragana)(consumed . t)"
  "(preedit . ((cursor . 9)"
  "(segment ((annotation . underline)(value . \"私の\")(value-length . 2)(key . \"わたしの\"))"
  "((annotation . highlight)(value . \"生絵は\")(value-length . 3)(key . \"なまえは\")))"
  "(highlighted-position . 2)))"
  "(candidates . ((focused-index . 10)(size . 17)"
  "(candidate ((index . 9)(value . \"名前は\")(annotation . ((description . \"ひらがな\")(shortcut . \"1\")))(id . -3))"
  "((index . 10)(value . \"生絵は\")(annotation . ((shortcut . \"2\")))(id . 1)))"
  "(page-size . 9))))))";

static const gchar passthrough_reply[] =
  "((emacs-event-id . 8)(emacs-session-id . 1)(output . ((consumed . nil)"
  "(key . ((key-code . 99)(modifier-keys ctrl))))))";

static void
test_sexp_reply (void)
{
  NimfMozcSexp       *reply = nimf_mozc_sexp_parse (conversion_reply);
  const NimfMozcSexp *output;
  const NimfMozcSexp *preedit;
  const NimfMozcSexp *segments;
  const NimfMozcSexp *candidates;
  const NimfMozcSexp *candidate;
  const NimfMozcSexp *first;

  g_assert_nonnull (reply);
  g_assert_cmpint (nimf_mozc_sexp_get_int (reply, "emacs-event-id", -1), ==, 58);

  output = nimf_mozc_sexp_lookup (reply, "output");
  g_assert_cmpstr (nimf_mozc_sexp_get_text (output, "consumed"), ==, "t");
  g_assert_cmpstr (nimf_mozc_sexp_get_text (output, "id"), ==, "9606594367642925272");

  preedit = nimf_mozc_sexp_lookup (output, "preedit");
  g_assert_cmpint (nimf_mozc_sexp_get_int (preedit, "cursor", -1), ==, 9);

  segments = nimf_mozc_sexp_lookup (preedit, "segment");
  g_assert_cmpuint (nimf_mozc_sexp_length (segments), ==, 3);
  g_assert_cmpstr (nimf_mozc_sexp_get_text (nimf_mozc_sexp_item (segments, 2), "value"),
                   ==, "生絵は");

  candidates = nimf_mozc_get_candidate_window (output);
  g_assert_cmpint (nimf_mozc_sexp_get_int (candidates, "focused-index", -1), ==, 10);
  g_assert_cmpint (nimf_mozc_sexp_get_int (candidates, "page-size", -1), ==, 9);

  candidate = nimf_mozc_sexp_lookup (candidates, "candidate");
  first     = nimf_mozc_sexp_item (candidate, 1);
  g_assert_cmpint (nimf_mozc_sexp_get_int (first, "index", -1), ==, 9);
  g_assert_cmpint (nimf_mozc_sexp_get_int (first, "id", 0), ==, -3);
  g_assert_cmpstr (nimf_mozc_sexp_get_text (nimf_mozc_sexp_lookup (first, "annotation"),
                                            "shortcut"), ==, "1");

  nimf_mozc_sexp_free (reply);
}

static void
test_candidate_window (void)
{
  /* Mozc 3.34, reduced to the fields used by the candidate UI. */
  NimfMozcSexp *output = nimf_mozc_sexp_parse (
    "((candidate-window . ((focused-index . 1)(size . 10)(page-size . 9)"
    "(candidate ((index . 0)(value . \"日本語\")(id . 0))"
    "((index . 1)(value . \"ニホンゴ\")(id . 1))))))");
  const NimfMozcSexp *window = nimf_mozc_get_candidate_window (output);
  const NimfMozcSexp *candidates = nimf_mozc_sexp_lookup (window, "candidate");

  g_assert_cmpint (nimf_mozc_sexp_get_int (window, "focused-index", -1), ==, 1);
  g_assert_cmpint (nimf_mozc_sexp_get_int (window, "size", 0), ==, 10);
  g_assert_cmpint (nimf_mozc_sexp_get_int (window, "page-size", 0), ==, 9);
  g_assert_cmpstr (nimf_mozc_sexp_get_text (nimf_mozc_sexp_item (candidates, 2),
                                          "value"), ==, "ニホンゴ");
  nimf_mozc_sexp_free (output);

  /* all-candidate-words is not a visible, selectable candidate window. */
  output = nimf_mozc_sexp_parse (
    "((all-candidate-words . ((candidates ((id . 0)(value . \"日本語\"))))))");
  g_assert_null (nimf_mozc_get_candidate_window (output));
  nimf_mozc_sexp_free (output);
}

static void
test_sexp_forms (void)
{
  NimfMozcSexp       *sexp;
  const NimfMozcSexp *key;

  sexp = nimf_mozc_sexp_parse (passthrough_reply);
  g_assert_nonnull (sexp);
  key = nimf_mozc_sexp_lookup (nimf_mozc_sexp_lookup (sexp, "output"), "key");
  g_assert_cmpint (nimf_mozc_sexp_get_int (key, "key-code", -1), ==, 99);
  g_assert_cmpstr (nimf_mozc_sexp_get_text (key, "modifier-keys"), ==, "ctrl");
  nimf_mozc_sexp_free (sexp);

  sexp = nimf_mozc_sexp_parse ("((output . ()))");
  g_assert_nonnull (sexp);
  g_assert_cmpuint (nimf_mozc_sexp_length (nimf_mozc_sexp_lookup (sexp, "output")), ==, 1);
  g_assert_null (nimf_mozc_sexp_get_text (sexp, "output"));
  nimf_mozc_sexp_free (sexp);

  sexp = nimf_mozc_sexp_parse ("((value . \"a\\\"b\\\\c\"))");
  g_assert_nonnull (sexp);
  g_assert_cmpstr (nimf_mozc_sexp_get_text (sexp, "value"), ==, "a\"b\\c");
  nimf_mozc_sexp_free (sexp);
}

static void
test_sexp_malformed (void)
{
  const gchar *inputs[] = {
    "", "(", ")", "((a . 1)", "(a . )", "(a . 1 2)", "(\"open)", "(a) b"
  };
  GString *deep = g_string_new (NULL);
  guint    i;

  for (i = 0; i < G_N_ELEMENTS (inputs); i++)
    g_assert_null (nimf_mozc_sexp_parse (inputs[i]));

  for (i = 0; i < 100000; i++)
    g_string_append_c (deep, '(');

  g_assert_null (nimf_mozc_sexp_parse (deep->str));
  g_string_free (deep, TRUE);
}

static void
assert_tokens (guint32 keyval, guint32 state, const gchar *expected)
{
  gchar *tokens = nimf_mozc_key_to_tokens (keyval, state);

  g_assert_cmpstr (tokens, ==, expected);
  g_free (tokens);
}

static void
test_key_tokens (void)
{
  assert_tokens (NIMF_KEY_a, 0, "97");
  assert_tokens (NIMF_KEY_A, NIMF_SHIFT_MASK, "65");
  assert_tokens (NIMF_KEY_a, NIMF_CONTROL_MASK, "ctrl 97");
  assert_tokens (NIMF_KEY_a, NIMF_MOD1_MASK, "alt 97");
  assert_tokens (NIMF_KEY_space, 0, "space");
  assert_tokens (NIMF_KEY_space, NIMF_SHIFT_MASK, "shift space");
  assert_tokens (NIMF_KEY_Return, 0, "enter");
  assert_tokens (NIMF_KEY_KP_Enter, 0, "enter");
  assert_tokens (NIMF_KEY_Page_Down, 0, "pagedown");
  assert_tokens (NIMF_KEY_F7, 0, "f7");
  assert_tokens (NIMF_KEY_ISO_Left_Tab, NIMF_SHIFT_MASK, "shift tab");

  assert_tokens (NIMF_KEY_Shift_L, 0, NULL);
  assert_tokens (NIMF_KEY_a, NIMF_SUPER_MASK, NULL);
  assert_tokens (0x20ac, 0, NULL);
}

static NimfMozcHelper *
fake_helper_new (const gchar *mode, gint timeout_ms, gint retry_delay_ms)
{
  g_setenv ("FAKE_MODE", mode, TRUE);

  return nimf_mozc_helper_new (FAKE_HELPER, timeout_ms, retry_delay_ms);
}

static void
test_helper_send_key (void)
{
  NimfMozcHelper     *helper = fake_helper_new ("ok", 2000, 0);
  NimfMozcSexp       *reply;
  const NimfMozcSexp *output;
  gint                i;

  for (i = 0; i < 3; i++)
  {
    reply = nimf_mozc_helper_send_key (helper, "97");
    g_assert_nonnull (reply);
    output = nimf_mozc_sexp_lookup (reply, "output");
    g_assert_cmpstr (nimf_mozc_sexp_get_text (output, "echo"), ==, "7 97)");
    nimf_mozc_sexp_free (reply);
  }

  nimf_mozc_helper_free (helper);
}

static void
assert_mode_fails (const gchar *mode)
{
  NimfMozcHelper *helper = fake_helper_new (mode, 200, 0);

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*nimf-mozc:*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  nimf_mozc_helper_free (helper);
}

static void
test_helper_failures (void)
{
  assert_mode_fails ("exit");
  assert_mode_fails ("hang");
  assert_mode_fails ("garbage");
  assert_mode_fails ("error");
  assert_mode_fails ("wrong-event");
}

static void
test_helper_missing_program (void)
{
  NimfMozcHelper *helper;

  helper = nimf_mozc_helper_new ("/nonexistent/mozc_emacs_helper", 200, 0);

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*nimf-mozc:*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  nimf_mozc_helper_free (helper);
}

/* The delay is long on purpose: an exit must not wait for it. */
static void
test_helper_restarts_after_exit (void)
{
  NimfMozcHelper *helper = fake_helper_new ("exit", 500, 60000);
  NimfMozcSexp   *reply;

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*nimf-mozc:*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  g_setenv ("FAKE_MODE", "ok", TRUE);
  reply = nimf_mozc_helper_send_key (helper, "97");
  g_assert_nonnull (reply);
  nimf_mozc_sexp_free (reply);

  nimf_mozc_helper_free (helper);
}

static void
test_helper_timeout_delays_retry (void)
{
  NimfMozcHelper *helper = fake_helper_new ("hang", 200, 60000);

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*timed out*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  /* No warning here: the delay blocks the restart without touching a process. */
  g_setenv ("FAKE_MODE", "ok", TRUE);
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));

  nimf_mozc_helper_free (helper);
}

static void
test_helper_start_failure_delays_retry (void)
{
  NimfMozcHelper *helper;

  helper = nimf_mozc_helper_new ("/nonexistent/mozc_emacs_helper", 200, 60000);

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*nimf-mozc:*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));

  nimf_mozc_helper_free (helper);
}

/* mozc_server inherits the helper pipes, so a dead helper gives no EOF.
 * Waiting for the timeout here would freeze all input in the nimf server. */
static void
test_helper_detects_exit_with_open_pipe (void)
{
  NimfMozcHelper *helper = fake_helper_new ("orphan", 10000, 0);
  gint64          start  = g_get_monotonic_time ();

  g_test_expect_message ("nimf", G_LOG_LEVEL_WARNING, "*process exited*");
  g_assert_null (nimf_mozc_helper_send_key (helper, "97"));
  g_test_assert_expected_messages ();

  g_assert_cmpint (g_get_monotonic_time () - start, <, 2 * G_TIME_SPAN_SECOND);

  nimf_mozc_helper_free (helper);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/mozc/sexp/reply", test_sexp_reply);
  g_test_add_func ("/mozc/protocol/candidate-window", test_candidate_window);
  g_test_add_func ("/mozc/sexp/forms", test_sexp_forms);
  g_test_add_func ("/mozc/sexp/malformed", test_sexp_malformed);
  g_test_add_func ("/mozc/key/tokens", test_key_tokens);
  g_test_add_func ("/mozc/helper/send-key", test_helper_send_key);
  g_test_add_func ("/mozc/helper/failures", test_helper_failures);
  g_test_add_func ("/mozc/helper/missing-program", test_helper_missing_program);
  g_test_add_func ("/mozc/helper/restarts-after-exit", test_helper_restarts_after_exit);
  g_test_add_func ("/mozc/helper/timeout-delays-retry", test_helper_timeout_delays_retry);
  g_test_add_func ("/mozc/helper/start-failure-delays-retry",
                   test_helper_start_failure_delays_retry);
  g_test_add_func ("/mozc/helper/exit-with-open-pipe",
                   test_helper_detects_exit_with_open_pipe);

  return g_test_run ();
}
