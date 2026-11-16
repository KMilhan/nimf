/* NimfIM client behavior against a private in-process server. */
#include "nimf.h"
#include "nimf-message-private.h"
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>

typedef struct
{
  GSocket *listener;
  GSocket *connection;
  GThread *thread;
  GMutex   lock;
  GArray  *cursor_areas;
  guint    created;
} FakeServer;

static gchar *socket_path;

static gpointer
serve (gpointer data)
{
  FakeServer *server = data;
  GSocket *connection = g_socket_accept (server->listener, NULL, NULL);
  g_assert_nonnull (connection);
  g_mutex_lock (&server->lock);
  server->connection = g_object_ref (connection);
  g_mutex_unlock (&server->lock);

  NimfMessage *message;
  while ((message = nimf_recv_message (connection)))
  {
    gboolean handled = FALSE;
    gboolean boolean_reply = message->header.type == NIMF_MESSAGE_FILTER_EVENT;
    g_mutex_lock (&server->lock);
    if (message->header.type == NIMF_MESSAGE_CREATE_CONTEXT)
      server->created++;
    else if (message->header.type == NIMF_MESSAGE_SET_CURSOR_LOCATION)
      g_array_append_vals (server->cursor_areas, message->data, 1);
    g_mutex_unlock (&server->lock);
    g_assert_true (nimf_send_message (connection, message->header.icid,
                                      message->header.type + 1,
                                      boolean_reply ? &handled : NULL,
                                      boolean_reply ? sizeof handled : 0, NULL));
    nimf_message_unref (message);
  }
  g_object_unref (connection);
  return NULL;
}

static FakeServer *
fake_server_start (void)
{
  FakeServer *server = g_new0 (FakeServer, 1);
  g_autoptr(GSocketAddress) address =
    g_unix_socket_address_new_with_type (socket_path, -1, G_UNIX_SOCKET_ADDRESS_PATH);
  g_mutex_init (&server->lock);
  server->cursor_areas = g_array_new (FALSE, FALSE, sizeof (NimfRectangle));
  server->listener = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM,
                                   G_SOCKET_PROTOCOL_DEFAULT, NULL);
  g_assert_true (g_socket_bind (server->listener, address, FALSE, NULL));
  g_assert_true (g_socket_listen (server->listener, NULL));
  server->thread = g_thread_new ("fake-nimf", serve, server);
  return server;
}

/* Joins after the client closes, or after shutdown() forces a disconnect. */
static void
fake_server_stop (FakeServer *server, gboolean disconnect)
{
  if (disconnect)
  {
    g_mutex_lock (&server->lock);
    g_assert_true (g_socket_shutdown (server->connection, TRUE, TRUE, NULL));
    g_mutex_unlock (&server->lock);
  }
  g_thread_join (server->thread);
  g_clear_object (&server->connection);
  g_object_unref (server->listener);
  g_assert_cmpint (g_unlink (socket_path), ==, 0);
  g_array_unref (server->cursor_areas);
  g_mutex_clear (&server->lock);
  g_free (server);
}

static guint
cursor_count (FakeServer *server)
{
  g_mutex_lock (&server->lock);
  guint count = server->cursor_areas->len;
  g_mutex_unlock (&server->lock);
  return count;
}

static void
assert_last_cursor (FakeServer *server, const NimfRectangle *expected)
{
  g_mutex_lock (&server->lock);
  g_assert_cmpuint (server->cursor_areas->len, >, 0);
  NimfRectangle last = g_array_index (server->cursor_areas, NimfRectangle,
                                      server->cursor_areas->len - 1);
  g_mutex_unlock (&server->lock);
  g_assert_cmpmem (&last, sizeof last, expected, sizeof *expected);
}

static void
test_unchanged_cursor_location_is_sent_once (void)
{
  const NimfRectangle first  = { 10, 20, 0, 18 };
  const NimfRectangle second = { 24, 20, 0, 18 };
  FakeServer *server = fake_server_start ();
  NimfIM *im = nimf_im_new ();

  nimf_im_set_cursor_location (im, &first);
  nimf_im_set_cursor_location (im, &first);
  g_assert_cmpuint (cursor_count (server), ==, 1);
  assert_last_cursor (server, &first);

  nimf_im_set_cursor_location (im, &second);
  nimf_im_set_cursor_location (im, &second);
  nimf_im_set_cursor_location (im, &first);
  g_assert_cmpuint (cursor_count (server), ==, 3);
  assert_last_cursor (server, &first);

  g_object_unref (im);
  fake_server_stop (server, FALSE);
}

static void
test_reconnect_resends_cursor_location (void)
{
  const NimfRectangle area = { 10, 20, 0, 18 };
  FakeServer *server = fake_server_start ();
  NimfIM *im = nimf_im_new ();
  nimf_im_set_cursor_location (im, &area);
  g_assert_cmpuint (cursor_count (server), ==, 1);

  /* Let the client observe the hangup before the socket file reappears. */
  fake_server_stop (server, TRUE);
  while (g_main_context_iteration (NULL, FALSE))
    ;

  /* The client reconnects when its socket monitor sees the new server. */
  server = fake_server_start ();
  gint64 deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  for (;;)
  {
    g_mutex_lock (&server->lock);
    gboolean created = server->created == 1;
    g_mutex_unlock (&server->lock);
    if (created)
      break;
    g_assert_cmpint (g_get_monotonic_time (), <, deadline);
    g_main_context_iteration (NULL, FALSE);
    g_usleep (10 * G_TIME_SPAN_MILLISECOND);
  }

  nimf_im_set_cursor_location (im, &area);
  g_assert_cmpuint (cursor_count (server), ==, 1);
  assert_last_cursor (server, &area);

  g_object_unref (im);
  fake_server_stop (server, FALSE);
}

int
main (int argc, char **argv)
{
  /* Read once by GLib; set before any runtime-directory lookup. */
  g_autofree gchar *runtime = g_dir_make_tmp ("nimf-test-im-XXXXXX", NULL);
  g_assert_nonnull (runtime);
  g_setenv ("XDG_RUNTIME_DIR", runtime, TRUE);
  g_autofree gchar *directory = g_build_filename (runtime, "nimf", NULL);
  g_assert_cmpint (g_mkdir (directory, 0700), ==, 0);
  socket_path = g_build_filename (directory, "socket", NULL);

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/im/unchanged-cursor-location-is-sent-once",
                   test_unchanged_cursor_location_is_sent_once);
  g_test_add_func ("/im/reconnect-resends-cursor-location",
                   test_reconnect_resends_cursor_location);
  gint status = g_test_run ();

  g_rmdir (directory);
  g_rmdir (runtime);
  g_free (socket_path);
  return status;
}
