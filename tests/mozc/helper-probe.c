#include "nimf-mozc-helper.h"
#include "nimf-mozc-protocol.h"

static NimfMozcSexp *
send_key (NimfMozcHelper *helper, const char *key)
{
  NimfMozcSexp *reply = nimf_mozc_helper_send_key (helper, key);
  g_assert_nonnull (reply);
  return reply;
}

int
main (void)
{
  NimfMozcHelper *helper = nimf_mozc_helper_new ("mozc_emacs_helper", 3000, 30000);
  NimfMozcSexp *reply = NULL;
  const char *input = "nihongo";
  for (const char *key = input; *key; key++)
  {
    char token[4];
    g_snprintf (token, sizeof token, "%u", (unsigned char) *key);
    nimf_mozc_sexp_free (reply);
    reply = send_key (helper, token);
  }
  const NimfMozcSexp *output = nimf_mozc_sexp_lookup (reply, "output");
  const NimfMozcSexp *preedit = nimf_mozc_sexp_lookup (output, "preedit");
  const NimfMozcSexp *segments = nimf_mozc_sexp_lookup (preedit, "segment");
  GString *text = g_string_new (NULL);
  for (guint i = 1; i < nimf_mozc_sexp_length (segments); i++)
    g_string_append (text, nimf_mozc_sexp_get_text (nimf_mozc_sexp_item (segments, i), "value"));
  g_assert_cmpstr (text->str, ==, "にほんご");
  g_string_free (text, TRUE);
  nimf_mozc_sexp_free (reply);

  reply = send_key (helper, "space");
  output = nimf_mozc_sexp_lookup (reply, "output");
  g_assert_nonnull (nimf_mozc_sexp_lookup (output, "all-candidate-words"));
  nimf_mozc_sexp_free (reply);
  reply = send_key (helper, "space");
  output = nimf_mozc_sexp_lookup (reply, "output");
  const NimfMozcSexp *candidates = nimf_mozc_get_candidate_window (output);
  int status = 0;
  if (nimf_mozc_sexp_get_int (candidates, "size", 0) <= 0)
  {
    g_printerr ("FAIL engine protocol: missing candidate window\n");
    status = 1;
  }
  nimf_mozc_sexp_free (reply);

  reply = send_key (helper, "up");
  nimf_mozc_sexp_free (reply);

  reply = send_key (helper, "enter");
  output = nimf_mozc_sexp_lookup (reply, "output");
  const NimfMozcSexp *result = nimf_mozc_sexp_lookup (output, "result");
  g_assert_cmpstr (nimf_mozc_sexp_get_text (result, "value"), ==, "日本語");
  nimf_mozc_sexp_free (reply);
  nimf_mozc_helper_free (helper);
  g_print ("PASS real helper preedit and commit\n");
  return status;
}
