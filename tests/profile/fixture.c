#include "nimf-profile-private.h"
#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static volatile guint counter;

static void
nested (void)
{
  NIMF_PROFILE_SCOPE (NIMF_PROFILE_CLIENT_FILTER, 0);
  {
    NIMF_PROFILE_SCOPE (NIMF_PROFILE_CLIENT_LOCK, 0);
    g_usleep (1000);
    return;
  }
}

static gpointer
worker (gpointer unused)
{
  (void) unused;
  for (guint i = 0; i < 20; i++)
    nested ();
  return NULL;
}

static guint64
now (void)
{
  struct timespec value;
  clock_gettime (CLOCK_MONOTONIC, &value);
  return (guint64) value.tv_sec * 1000000000 + value.tv_nsec;
}

int
main (int argc, char **argv)
{
  if (argc != 2)
    return 2;
  if (g_str_equal (argv[1], "threads"))
  {
    GThread *threads[4];
    for (guint i = 0; i < G_N_ELEMENTS (threads); i++)
      threads[i] = g_thread_new ("profile-fixture", worker, NULL);
    for (guint i = 0; i < G_N_ELEMENTS (threads); i++)
      g_thread_join (threads[i]);
    nested ();
  }
  else if (g_str_equal (argv[1], "collision"))
  {
    g_autofree gchar *filename = g_strdup_printf ("%s/nimf-%d-1.csv",
                                                   g_getenv ("NIMF_PROFILE_DIR"), getpid ());
    if (!g_file_set_contents (filename, "preserved\n", -1, NULL))
      return 1;
    nested ();
  }
  else if (g_str_equal (argv[1], "calibrate") || g_str_equal (argv[1], "overflow"))
  {
    guint count = g_str_equal (argv[1], "calibrate") ? 100000 : 300;
    /* Initialize the collector before measuring steady per-span overhead. */
    nested ();
    guint64 start = now ();
    for (guint i = 0; i < count; i++)
    {
      NIMF_PROFILE_SCOPE (NIMF_PROFILE_HANGUL_PROCESS, 0);
      counter++;
    }
    printf ("%.3f\n", (now () - start) / (double) count);
  }
  else
    nested ();
  return 0;
}
