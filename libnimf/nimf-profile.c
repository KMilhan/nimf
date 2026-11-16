/* Bounded, per-thread profiling for isolated diagnostic builds. */
#include "nimf-profile-private.h"

#ifdef NIMF_ENABLE_PROFILING
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <glib/gstdio.h>

typedef struct
{
  guint64 start_ns, end_ns, cpu_start_ns, cpu_end_ns;
  guint parent;
  NimfProfilePoint point;
  gint detail;
} Record;

typedef struct
{
  Record *records;
  guint used, capacity, top, dropped, clock_errors;
  gint pid, thread;
  gboolean cpu;
  FILE *output;
} Profile;

static const gchar *point_names[] = {
  "client.filter", "client.lock", "client.request", "client.dispatch",
  "server.filter", "engine.filter", "server.emit", "ipc.send", "ipc.receive",
  "ipc.wait", "hangul.process", "mozc.key", "mozc.start", "mozc.request",
  "mozc.read", "mozc.parse", "client.state", "client.signal", "gtk.preedit",
  "gtk.cursor"
};
G_STATIC_ASSERT (G_N_ELEMENTS (point_names) == NIMF_PROFILE_N_POINTS);

static void profile_free (gpointer data);
static GPrivate current = G_PRIVATE_INIT (profile_free);
static gint thread_sequence;

static guint64
read_clock (Profile *profile, clockid_t clock)
{
  struct timespec value = { 0 };
  if (clock_gettime (clock, &value) != 0)
    profile->clock_errors++;
  return (guint64) value.tv_sec * 1000000000 + value.tv_nsec;
}

static Profile *
profile_new (void)
{
  Profile *profile = g_new0 (Profile, 1);
  const gchar *directory = g_getenv ("NIMF_PROFILE_DIR");
  profile->pid = getpid ();
  if (directory == NULL || *directory == '\0')
    return profile;

  profile->capacity = 65536;
  const gchar *capacity = g_getenv ("NIMF_PROFILE_CAPACITY");
  if (capacity)
  {
    gchar *end;
    guint64 value = g_ascii_strtoull (capacity, &end, 10);
    if (*capacity == '\0' || *end || value < 256 || value > 1048576)
    {
      g_printerr ("Nimf profiling: capacity must be between 256 and 1048576\n");
      return profile;
    }
    profile->capacity = value;
  }
  profile->cpu = g_strcmp0 (g_getenv ("NIMF_PROFILE_CPU"), "1") == 0;
  profile->thread = g_atomic_int_add (&thread_sequence, 1) + 1;
  g_autofree gchar *name = g_strdup_printf ("nimf-%d-%d.csv", profile->pid, profile->thread);
  g_autofree gchar *filename = g_build_filename (directory, name, NULL);
  gint fd = g_open (filename, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0)
  {
    g_printerr ("Nimf profiling: cannot create trace: %s\n", g_strerror (errno));
    return profile;
  }
  profile->output = fdopen (fd, "w");
  if (profile->output == NULL)
  {
    close (fd);
    g_printerr ("Nimf profiling: cannot open trace stream\n");
    return profile;
  }
  /* Preallocate once. No allocation or output occurs for individual spans. */
  profile->records = g_new0 (Record, profile->capacity);
  volatile gchar *pages = (volatile gchar *) profile->records;
  gsize page_size = sysconf (_SC_PAGESIZE);
  for (gsize offset = 0; offset < sizeof (Record) * profile->capacity; offset += page_size)
    pages[offset] = 0;
  return profile;
}

void
nimf_profile_begin (NimfProfileScope *scope, NimfProfilePoint point, gint detail)
{
  Profile *profile = g_private_get (&current);
  if (G_UNLIKELY (profile == NULL))
  {
    profile = profile_new ();
    g_private_set (&current, profile);
  }
  if (profile->output == NULL)
    return;
  if (profile->used == profile->capacity)
  {
    profile->dropped++;
    return;
  }
  g_return_if_fail (point >= 0 && point < NIMF_PROFILE_N_POINTS);
  scope->state = profile;
  scope->index = profile->used++;
  Record *record = &profile->records[scope->index];
  record->point = point;
  record->detail = detail;
  record->parent = profile->top;
  profile->top = scope->index + 1;
  record->start_ns = read_clock (profile, CLOCK_MONOTONIC);
  if (profile->cpu)
    record->cpu_start_ns = read_clock (profile, CLOCK_THREAD_CPUTIME_ID);
}

void
nimf_profile_end (NimfProfileScope *scope)
{
  Profile *profile = scope->state;
  if (profile == NULL)
    return;
  Record *record = &profile->records[scope->index];
  if (profile->cpu)
    record->cpu_end_ns = read_clock (profile, CLOCK_THREAD_CPUTIME_ID);
  record->end_ns = read_clock (profile, CLOCK_MONOTONIC);
  profile->top = record->parent;
  scope->state = NULL;
}

static void
profile_free (gpointer data)
{
  Profile *profile = data;
  if (profile->output)
  {
    if (profile->pid == getpid ())
    {
      guint incomplete = 0;
      for (guint i = 0; i < profile->used; i++)
        incomplete += profile->records[i].end_ns == 0;
      fprintf (profile->output,
               "# nimf-profile-v1 pid=%d thread=%d cpu=%d records=%u dropped=%u incomplete=%u clock_errors=%u\n",
               profile->pid, profile->thread, profile->cpu, profile->used, profile->dropped,
               incomplete, profile->clock_errors);
      fputs ("id,parent,span,detail,start_ns,end_ns,cpu_start_ns,cpu_end_ns\n", profile->output);
      for (guint i = 0; i < profile->used; i++)
      {
        Record *r = &profile->records[i];
        fprintf (profile->output, "%u,%u,%s,%d,%" G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT
                 ",%" G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT "\n",
                 i + 1, r->parent, point_names[r->point], r->detail,
                 r->start_ns, r->end_ns, r->cpu_start_ns, r->cpu_end_ns);
      }
    }
    gboolean failed = ferror (profile->output);
    if (fclose (profile->output) != 0 || failed)
      g_printerr ("Nimf profiling: trace write failed\n");
  }
  g_free (profile->records);
  g_free (profile);
}

void
nimf_profile_flush (void)
{
  Profile *profile = g_private_get (&current);
  /* Flush only after leaving all instrumented scopes on this thread. */
  g_return_if_fail (profile == NULL || profile->top == 0);
  g_private_replace (&current, NULL);
}

static void __attribute__((destructor))
profile_shutdown (void)
{
  /* Worker threads flush through GPrivate; normal exit flushes this thread. */
  nimf_profile_flush ();
}
#endif
