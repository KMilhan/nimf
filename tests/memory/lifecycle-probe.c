#include <nimf.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <unistd.h>
#include <signal.h>

static gboolean peer_stopped;

static void
stop_peer_on_commit (NimfIM *im, const char *text, gpointer data)
{
  g_assert_true (text[0] != '\0');
  pid_t peer = g_ascii_strtoll (g_getenv ("NIMF_TEST_SERVER_PID"), NULL, 10);
  g_assert_cmpint (peer, >, 1);
  g_assert_cmpint (kill (peer, SIGKILL), ==, 0);
  /* Let the peer close before dispatch sends its COMMIT_REPLY. */
  g_usleep (100000);
  peer_stopped = TRUE;
}

static void
send_key (NimfIM *im, guint keyval)
{
  NimfEvent event = { .key = { .type = NIMF_EVENT_KEY_PRESS,
                              .keyval = keyval } };
  nimf_im_filter_event (im, &event);
}

static void
compose (NimfIM *im)
{
  nimf_im_focus_in (im);
  for (const char *key = "nihongo"; *key; key++)
    send_key (im, *key);
}

static void
release_on_commit (NimfIM *im, const char *text, NimfIM **owner)
{
  g_assert_true (im == *owner);
  g_assert_true (text[0] != '\0');
  g_clear_object (owner);
}

static void
drain_settings (void)
{
  g_settings_sync ();
  gint64 until = g_get_monotonic_time () + 250 * G_TIME_SPAN_MILLISECOND;
  do
  {
    while (g_main_context_iteration (NULL, FALSE));
    g_usleep (1000);
  } while (g_get_monotonic_time () < until);
}

static void
check_method_infos (const char *directory)
{
  const char *engines[] = { "anthy", "libhangul" };
  for (guint i = 0; i < G_N_ELEMENTS (engines); i++)
  {
    g_autofree char *name = g_strdup_printf ("libnimf-%s.so", engines[i]);
    g_autofree char *path = g_build_filename (directory, name, NULL);
    g_autofree char *symbol = g_strdup_printf ("nimf_%s_get_method_infos", engines[i]);
    GModule *module = g_module_open (path, G_MODULE_BIND_LAZY);
    g_assert_nonnull (module);
    NimfMethodInfo **(*get_infos) (void) = NULL;
    g_assert_true (g_module_symbol (module, symbol, (gpointer *) &get_infos));
    for (guint iteration = 0; iteration < 20; iteration++)
    {
      NimfMethodInfo **infos = get_infos ();
      g_assert_nonnull (infos[0]);
      for (guint j = 0; infos[j]; j++)
        nimf_method_info_free (infos[j]);
      g_free (infos);
    }
    g_module_close (module);
  }
}

static gpointer
cycle_contexts (gpointer data)
{
  for (guint i = 0; i < 10; i++)
  {
    NimfIM *im = nimf_im_new ();
    nimf_im_focus_in (im);
    nimf_im_reset (im);
    nimf_im_focus_out (im);
    g_object_unref (im);
  }
  g_atomic_int_inc ((gint *) data);
  return NULL;
}

int
main (int argc, char **argv)
{
  g_assert_cmpint (argc, >=, 2);
  if (g_str_equal (argv[1], "methods"))
  {
    g_assert_cmpint (argc, ==, 3);
    check_method_infos (argv[2]);
  }
  else if (g_str_equal (argv[1], "repeat"))
  {
    for (guint i = 0; i < 20; i++)
    {
      NimfIM *first = nimf_im_new ();
      NimfIM *second = nimf_im_new ();
      compose (first);
      nimf_im_reset (first);
      g_object_unref (first);
      compose (second);
      nimf_im_reset (second);
      g_object_unref (second);
    }
  }
  else if (g_str_equal (argv[1], "preedit-toggle"))
  {
    NimfIM *im = nimf_im_new ();
    compose (im);
    g_autofree gchar *before = NULL;
    g_autofree gchar *after = NULL;
    nimf_im_get_preedit_string (im, &before, NULL, NULL);
    g_assert_true (before[0] != '\0');
    nimf_im_set_use_preedit (im, FALSE);
    nimf_im_set_use_preedit (im, TRUE);
    nimf_im_get_preedit_string (im, &after, NULL, NULL);
    g_assert_cmpstr (before, ==, after);
    nimf_im_reset (im);
    g_object_unref (im);
  }
  else if (g_str_equal (argv[1], "threads"))
  {
    gint finished = 0;
    GThread *workers[3];
    for (guint i = 0; i < G_N_ELEMENTS (workers); i++)
      workers[i] = g_thread_new ("input-context", cycle_contexts, &finished);
    while (g_atomic_int_get (&finished) != G_N_ELEMENTS (workers))
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
    for (guint i = 0; i < G_N_ELEMENTS (workers); i++)
      g_thread_join (workers[i]);
  }
  else if (g_str_equal (argv[1], "reentrant"))
  {
    NimfIM *owner = nimf_im_new ();
    g_signal_connect (owner, "commit", G_CALLBACK (release_on_commit), &owner);
    compose (owner);
    send_key (owner, NIMF_KEY_Return);
    g_assert_null (owner);
  }
  else if (g_str_equal (argv[1], "disconnect"))
  {
    compose (nimf_im_new ());
    /* Deliberately abandon a live composition to exercise server cancellation. */
    _exit (0);
  }
  else if (g_str_equal (argv[1], "server-disconnect"))
  {
    NimfIM *im = nimf_im_new ();
    g_signal_connect (im, "commit", G_CALLBACK (stop_peer_on_commit), NULL);
    compose (im);
    send_key (im, NIMF_KEY_Return);
    g_assert_true (peer_stopped);
    g_object_unref (im);
    while (g_main_context_iteration (NULL, FALSE));
  }
  else if (g_str_equal (argv[1], "unload"))
  {
    g_autoptr(GSettings) general = g_settings_new ("org.nimf");
    g_autoptr(GSettings) engine = g_settings_new ("org.nimf.engines.nimf-mozc");
    g_autoptr(GSettings) engines = g_settings_new ("org.nimf.engines");
    for (guint i = 0; i < 2; i++)
    {
      g_settings_set_boolean (general, "use-singleton", FALSE);
      g_settings_set_boolean (engine, "active-engine", TRUE);
      const char *active[] = { "nimf-system-keyboard", "nimf-mozc", NULL };
      g_settings_set_strv (general, "hidden-active-engines", active);
      g_settings_set_string (engines, "default-engine", "nimf-mozc");
      drain_settings ();
      NimfIM *im = nimf_im_new ();
      compose (im);
      if (i == 1)
        g_settings_set_boolean (general, "use-singleton", TRUE);
      drain_settings ();
      g_settings_set_boolean (engine, "active-engine", FALSE);
      const char *inactive[] = { "nimf-system-keyboard", NULL };
      g_settings_set_strv (general, "hidden-active-engines", inactive);
      drain_settings ();
      nimf_im_reset (im);
      send_key (im, 'a');
      g_object_unref (im);
    }
  }
  else
    g_error ("Unknown lifecycle scenario: %s", argv[1]);
  g_print ("PASS lifecycle %s\n", argv[1]);
  return 0;
}
