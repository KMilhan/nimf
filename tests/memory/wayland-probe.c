#include <nimf.h>
#include "nimf-module-private.h"
#include <wayland-server.h>

typedef struct
{
  struct wl_display *display;
  gint stop;
} Compositor;

static gpointer
serve (gpointer data)
{
  Compositor *compositor = data;
  struct wl_event_loop *loop = wl_display_get_event_loop (compositor->display);
  while (!g_atomic_int_get (&compositor->stop))
  {
    wl_event_loop_dispatch (loop, 20);
    wl_display_flush_clients (compositor->display);
  }
  return NULL;
}

static guint
count_fds (void)
{
  g_autoptr(GDir) directory = g_dir_open ("/proc/self/fd", 0, NULL);
  guint count = 0;
  while (g_dir_read_name (directory))
    count++;
  return count;
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_assert_cmpint (argc, ==, 2);
  Compositor compositor = { .display = wl_display_create () };
  g_assert_nonnull (compositor.display);
  g_assert_cmpint (wl_display_add_socket (compositor.display, "memory-test"), ==, 0);
  g_setenv ("WAYLAND_DISPLAY", "memory-test", TRUE);
  g_setenv ("XDG_SESSION_TYPE", "wayland", TRUE);
  GThread *thread = g_thread_new ("compositor", serve, &compositor);
  /* Registered dynamic types keep their module for the process lifetime. */
  NimfModule *module = nimf_module_new (argv[1]);
  g_assert_true (g_type_module_use (G_TYPE_MODULE (module)));
  guint before = count_fds ();
  for (guint i = 0; i < 20; i++)
  {
    NimfService *service = g_object_new (module->type, NULL);
    g_test_expect_message ("nimf", G_LOG_LEVEL_CRITICAL, "*No input_method global*");
    g_assert_false (nimf_service_start (service));
    g_test_assert_expected_messages ();
    nimf_service_stop (service);
    nimf_service_stop (service);
    g_object_unref (service);
  }
  g_atomic_int_set (&compositor.stop, TRUE);
  g_thread_join (thread);
  wl_display_destroy_clients (compositor.display);
  /* Server-side sockets may have been pending when the last client closed. */
  g_assert_cmpuint (count_fds (), ==, before);
  wl_display_destroy (compositor.display);
  g_print ("PASS Wayland partial initialization and repeated stop\n");
  return 0;
}
