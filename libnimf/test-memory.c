#include "nimf-message-private.h"
#include <sys/socket.h>
#include <string.h>

static NimfMessage *
receive_body (NimfMessageType type, const gchar *body, guint16 length)
{
  gint fds[2];
  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);
  g_autoptr(GSocket) sender = g_socket_new_from_fd (fds[0], NULL);
  g_autoptr(GSocket) receiver = g_socket_new_from_fd (fds[1], NULL);
  NimfMessageHeader header = { .icid = 1, .type = type, .data_len = length };
  g_assert_cmpint (g_socket_send (sender, (gchar *) &header, sizeof header,
                                 NULL, NULL), ==, sizeof header);
  if (length)
    g_assert_cmpint (g_socket_send (sender, body, length, NULL, NULL), ==, length);
  g_socket_close (sender, NULL);
  return nimf_recv_message (receiver);
}

static guint destroyed;

static void
destroy_body (gpointer data)
{
  destroyed++;
  g_free (data);
}

static void
test_result_owns_reply (void)
{
  destroyed = 0;
  NimfResult *result = nimf_result_new ();
  result->reply = nimf_message_new_full (NIMF_MESSAGE_COMMIT, 1,
                                        g_strdup ("text"), 5, destroy_body);
  NimfMessage *retained = nimf_message_ref (result->reply);
  nimf_result_unref (result);
  g_assert_cmpuint (destroyed, ==, 0);
  nimf_message_unref (retained);
  g_assert_cmpuint (destroyed, ==, 1);
}

static void
test_replace_body (void)
{
  destroyed = 0;
  NimfMessage *message = nimf_message_new_full (NIMF_MESSAGE_COMMIT, 1,
                                             g_strdup ("old"), 4, destroy_body);
  nimf_message_set_body (message, g_strdup ("new"), 4, destroy_body);
  g_assert_cmpuint (destroyed, ==, 1);
  g_assert_cmpstr (nimf_message_get_body (message), ==, "new");
  nimf_message_unref (message);
  g_assert_cmpuint (destroyed, ==, 2);
}

static void
test_invalid_bodies (void)
{
  const NimfMessageType types[] = {
    NIMF_MESSAGE_FILTER_EVENT, NIMF_MESSAGE_SET_CURSOR_LOCATION,
    NIMF_MESSAGE_SET_USE_PREEDIT, NIMF_MESSAGE_SET_SURROUNDING,
    NIMF_MESSAGE_PREEDIT_CHANGED, NIMF_MESSAGE_COMMIT,
    NIMF_MESSAGE_DELETE_SURROUNDING, NIMF_MESSAGE_FILTER_EVENT_REPLY,
    NIMF_MESSAGE_RETRIEVE_SURROUNDING_REPLY,
    NIMF_MESSAGE_DELETE_SURROUNDING_REPLY
  };
  for (guint i = 0; i < G_N_ELEMENTS (types); i++)
    g_assert_null (receive_body (types[i], NULL, 0));
  g_assert_null (receive_body (NIMF_MESSAGE_COMMIT, "bad", 3));
  g_assert_null (receive_body (NIMF_MESSAGE_PREEDIT_CHANGED, "bad!tail", 8));
  g_assert_null (receive_body (NIMF_MESSAGE_CREATE_CONTEXT, "x", 1));
  g_assert_null (receive_body (G_MAXINT, NULL, 0));
}

static void
test_valid_bodies (void)
{
  g_autoptr(NimfMessage) commit = receive_body (NIMF_MESSAGE_COMMIT, "日本語", 10);
  g_assert_nonnull (commit);
  g_assert_cmpstr (nimf_message_get_body (commit), ==, "日本語");

  gchar preedit[2 + sizeof (NimfPreeditAttr) + sizeof (gint)] = { 'a', 0 };
  NimfPreeditAttr attr = { 0 };
  gint cursor = 1;
  memcpy (preedit + 2, &attr, sizeof attr);
  memcpy (preedit + 2 + sizeof attr, &cursor, sizeof cursor);
  g_autoptr(NimfMessage) message = receive_body (NIMF_MESSAGE_PREEDIT_CHANGED,
                                               preedit, sizeof preedit);
  g_assert_nonnull (message);
}

static void
test_truncated_body (void)
{
  gint fds[2];
  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);
  g_autoptr(GSocket) sender = g_socket_new_from_fd (fds[0], NULL);
  g_autoptr(GSocket) receiver = g_socket_new_from_fd (fds[1], NULL);
  NimfMessageHeader header = { .icid = 1, .type = NIMF_MESSAGE_COMMIT,
                               .data_len = 20 };
  g_socket_send (sender, (gchar *) &header, sizeof header, NULL, NULL);
  g_socket_send (sender, "short", 6, NULL, NULL);
  g_socket_close (sender, NULL);
  g_assert_null (nimf_recv_message (receiver));
}

static void
test_cancelled_wait (void)
{
  g_autoptr(NimfResult) result = nimf_result_new ();
  g_autoptr(GMainContext) context = g_main_context_new ();
  result->cancelled = TRUE;
  /* A disconnected peer must not enter a main loop with no reply source. */
  nimf_result_iteration_until (result, context, 1, NIMF_MESSAGE_COMMIT_REPLY);
}

static void
test_oversized_send (void)
{
  gint fds[2];
  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);
  g_autoptr(GSocket) sender = g_socket_new_from_fd (fds[0], NULL);
  g_autoptr(GSocket) receiver = g_socket_new_from_fd (fds[1], NULL);
  destroyed = 0;
  g_assert_false (nimf_send_message (sender, 1, NIMF_MESSAGE_COMMIT,
                                   g_malloc0 (G_MAXUINT16 + 1), G_MAXUINT16 + 1,
                                   destroy_body));
  g_assert_cmpuint (destroyed, ==, 1);
  g_assert_true (g_socket_is_closed (sender));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/memory/result-owns-reply", test_result_owns_reply);
  g_test_add_func ("/memory/replace-body", test_replace_body);
  g_test_add_func ("/memory/invalid-bodies", test_invalid_bodies);
  g_test_add_func ("/memory/valid-bodies", test_valid_bodies);
  g_test_add_func ("/memory/truncated-body", test_truncated_body);
  g_test_add_func ("/memory/cancelled-wait", test_cancelled_wait);
  g_test_add_func ("/memory/oversized-send", test_oversized_send);
  return g_test_run ();
}
