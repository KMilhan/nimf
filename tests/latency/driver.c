#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
  Window window;
  int x, y, width, height;
  gint64 injected_at;
  gint64 key_at, preedit_at, text_at;
  guint keyval;
  char *preedit, *text;
} Probe;

typedef struct
{
  const char *name;
  KeySym key;
  const char *preedit, *text;
} Step;

static const Step japanese_steps[] = {
  { "preedit", XK_a, "あ", "" },
  { "commit", XK_Return, "", "あ" },
  { "erase", XK_BackSpace, "", "" }
};

static const Step korean_steps[] = {
  { "preedit-initial", XK_g, "ㅎ", "" },
  { "preedit-vowel", XK_k, "하", "" },
  { "preedit-final", XK_s, "한", "" },
  { "commit", XK_Return, "", "한" },
  { "erase", XK_BackSpace, "", "" }
};

typedef struct
{
  gint64 injected, key, model, lower, upper, readback;
  guint polls;
} Sample;

static void image_free (XImage *image) { XDestroyImage (image); }
G_DEFINE_AUTOPTR_CLEANUP_FUNC (XImage, image_free)

static gboolean
drain (GIOChannel *channel, Probe *probe)
{
  while (TRUE)
  {
    g_autofree char *line = NULL;
    g_autoptr(GError) error = NULL;
    GIOStatus status = g_io_channel_read_line (channel, &line, NULL, NULL, &error);
    if (status == G_IO_STATUS_AGAIN)
      return TRUE;
    if (status != G_IO_STATUS_NORMAL)
    {
      g_printerr ("Probe output ended: %s\n", error ? error->message : "EOF");
      return FALSE;
    }
    g_strchomp (line);
    g_auto(GStrv) fields = g_strsplit (line, "\t", -1);
    guint count = g_strv_length (fields);
    if (count == 6 && g_str_equal (fields[0], "ready"))
    {
      probe->window = g_ascii_strtoull (fields[1], NULL, 10);
      probe->x = g_ascii_strtoll (fields[2], NULL, 10);
      probe->y = g_ascii_strtoll (fields[3], NULL, 10);
      probe->width = g_ascii_strtoll (fields[4], NULL, 10);
      probe->height = g_ascii_strtoll (fields[5], NULL, 10);
    }
    else if (count == 3 && g_str_equal (fields[0], "key"))
    {
      gint64 timestamp = g_ascii_strtoll (fields[1], NULL, 10);
      /* Some asynchronous IM modules forward the same key to GTK again. */
      if (!probe->key_at && timestamp >= probe->injected_at)
      {
        probe->key_at = timestamp;
        probe->keyval = g_ascii_strtoull (fields[2], NULL, 10);
      }
    }
    else if (count == 3 && (g_str_equal (fields[0], "preedit") ||
                            g_str_equal (fields[0], "text")))
    {
      gboolean preedit = g_str_equal (fields[0], "preedit");
      gsize size = 0;
      g_autofree guchar *decoded = g_base64_decode (fields[2], &size);
      char **destination = preedit ? &probe->preedit : &probe->text;
      g_free (*destination);
      *destination = g_strndup ((const char *) decoded, size);
      if (preedit)
        probe->preedit_at = g_ascii_strtoll (fields[1], NULL, 10);
      else
        probe->text_at = g_ascii_strtoll (fields[1], NULL, 10);
    }
    else
    {
      g_printerr ("Invalid probe record: %s\n", line);
      return FALSE;
    }
  }
}

static gboolean
same_image (XImage *a, XImage *b)
{
  return a->width == b->width && a->height == b->height &&
         a->bytes_per_line == b->bytes_per_line &&
         memcmp (a->data, b->data, (gsize) a->bytes_per_line * a->height) == 0;
}

static XImage *
capture (Display *display, int x, int y, guint width, guint height)
{
  return XGetImage (display, DefaultRootWindow (display), x, y, width, height,
                    AllPlanes, ZPixmap);
}

static gboolean
save_frame (XImage *image, const char *directory, const char *name)
{
  if (image->red_mask != 0xff0000 || image->green_mask != 0xff00 ||
      image->blue_mask != 0xff)
    return FALSE;
  g_autoptr(GString) ppm = g_string_new (NULL);
  g_string_printf (ppm, "P6\n%d %d\n255\n", image->width, image->height);
  for (int y = 0; y < image->height; y++)
    for (int x = 0; x < image->width; x++)
    {
      unsigned long pixel = XGetPixel (image, x, y);
      char rgb[] = { pixel >> 16, pixel >> 8, pixel };
      g_string_append_len (ppm, rgb, 3);
    }
  g_autofree char *filename = g_strdup_printf ("%s/%s.ppm", directory, name);
  g_autoptr(GError) error = NULL;
  if (!g_file_set_contents (filename, ppm->str, ppm->len, &error))
  {
    g_printerr ("Cannot save frame: %s\n", error->message);
    return FALSE;
  }
  return TRUE;
}

static gboolean
measure (Display *display, GIOChannel *channel, Probe *probe, const Step *step,
         int x, int y, guint width, guint height, guint poll_us, guint timeout_ms,
         Sample *sample, const char *snapshot_directory)
{
  if (!drain (channel, probe))
    return FALSE;
  g_autoptr(XImage) baseline = capture (display, x, y, width, height);
  g_autoptr(XImage) previous = capture (display, x, y, width, height);
  if (!baseline || !previous || !same_image (baseline, previous))
  {
    g_printerr ("Unstable baseline frame\n");
    return FALSE;
  }
  KeyCode code = XKeysymToKeycode (display, step->key);
  if (!code)
    return FALSE;
  probe->key_at = 0;
  probe->keyval = 0;
  sample->injected = g_get_monotonic_time ();
  probe->injected_at = sample->injected;
  if (!XTestFakeKeyEvent (display, code, TRUE, CurrentTime) ||
      !XTestFakeKeyEvent (display, code, FALSE, CurrentTime))
    return FALSE;
  XFlush (display);
  gint64 previous_start = sample->injected;
  gint64 deadline = sample->injected + (gint64) timeout_ms * 1000;
  while (g_get_monotonic_time () < deadline)
  {
    gint64 started = g_get_monotonic_time ();
    g_autoptr(XImage) current = capture (display, x, y, width, height);
    gint64 ended = g_get_monotonic_time ();
    sample->polls++;
    if (!current || !drain (channel, probe))
      return FALSE;
    if (!same_image (previous, current))
    {
      /* The snapshot occurs between request start and reply receipt. */
      sample->lower = previous_start;
      sample->upper = ended;
      sample->readback = ended - started;
      g_clear_pointer (&previous, image_free);
      previous = g_steal_pointer (&current);
    }
    previous_start = started;
    gboolean expected = g_strcmp0 (probe->preedit, step->preedit) == 0 &&
                        g_strcmp0 (probe->text, step->text) == 0;
    gint64 model_at = MAX (probe->preedit_at, probe->text_at);
    /* Confirm the final raster, rather than timing an intermediate repaint. */
    if (expected && model_at >= sample->injected && sample->upper >= model_at &&
        ended - sample->upper >= 25000 && !same_image (baseline, previous))
    {
      /* IM filters may consume the key before any widget key callback. */
      sample->key = probe->keyval == step->key ? probe->key_at : 0;
      sample->model = model_at;
      return !snapshot_directory || save_frame (previous, snapshot_directory, step->name);
    }
    g_usleep (poll_us);
  }
  g_printerr ("Timeout at %s: key=%u preedit='%s' text='%s' pixel_change=%d final_pixel_change=%d\n",
              step->name, probe->keyval, probe->preedit, probe->text,
              sample->upper != 0, !same_image (baseline, previous));
  return FALSE;
}

static gint
compare_sample (gconstpointer first, gconstpointer second)
{
  double a = *(const double *) first, b = *(const double *) second;
  return (a > b) - (a < b);
}

static double
percentile (GArray *values, guint percent)
{
  guint index = (values->len * percent + 99) / 100 - 1;
  return g_array_index (values, double, index);
}

static void
print_summary (const Step *step, GArray *samples)
{
  const char *names[] = { "key_ms", "model_ms", "screen_upper_ms", "interval_ms", "readback_ms" };
  for (guint metric = 0; metric < G_N_ELEMENTS (names); metric++)
  {
    g_autoptr(GArray) values = g_array_new (FALSE, FALSE, sizeof (double));
    for (guint i = 0; i < samples->len; i++)
    {
      Sample *sample = &g_array_index (samples, Sample, i);
      if (metric == 0 && !sample->key)
        continue;
      gint64 durations[] = { sample->key - sample->injected,
                             sample->model - sample->injected,
                             sample->upper - sample->injected,
                             sample->upper - sample->lower, sample->readback };
      double value = durations[metric] / 1000.0;
      g_array_append_val (values, value);
    }
    if (!values->len)
    {
      g_print ("%s n=0 %s unavailable\n", step->name, names[metric]);
      continue;
    }
    g_array_sort (values, compare_sample);
    g_print ("%s n=%u %s p50=%.3f p95=%.3f p99=%.3f max=%.3f\n",
             step->name, values->len, names[metric], percentile (values, 50),
             percentile (values, 95), percentile (values, 99), percentile (values, 100));
  }
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");
  gint iterations = 100, warmup = 10, poll_us = 500, timeout_ms = 5000;
  g_autofree char *probe_path = NULL;
  g_autofree char *output = NULL;
  g_autofree char *language_option = NULL;
  GOptionEntry options[] = {
    { "probe", 0, 0, G_OPTION_ARG_FILENAME, &probe_path, "GTK probe executable", "PATH" },
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &output, "Output directory", "PATH" },
    { "language", 0, 0, G_OPTION_ARG_STRING, &language_option, "Input workload: ja or ko", "LANG" },
    { "iterations", 0, 0, G_OPTION_ARG_INT, &iterations, "Measured cycles", "N" },
    { "warmup", 0, 0, G_OPTION_ARG_INT, &warmup, "Warmup cycles after the first cycle", "N" },
    { "poll-us", 0, 0, G_OPTION_ARG_INT, &poll_us, "Sleep between frame reads", "US" },
    { "timeout-ms", 0, 0, G_OPTION_ARG_INT, &timeout_ms, "Timeout per key", "MS" },
    { NULL }
  };
  g_autoptr(GOptionContext) options_context = g_option_context_new (NULL);
  g_option_context_add_main_entries (options_context, options, NULL);
  g_autoptr(GError) error = NULL;
  if (!g_option_context_parse (options_context, &argc, &argv, &error) || argc != 1 ||
      !probe_path || !output || iterations < 1 || iterations > 10000 ||
      warmup < 0 || warmup > 10000 || poll_us < 50 || poll_us > 10000 ||
      timeout_ms < 100 || timeout_ms > 60000)
  {
    g_printerr ("Invalid arguments: %s\n", error ? error->message : "see --help");
    return EXIT_FAILURE;
  }
  const char *language = language_option ? language_option : "ja";
  const Step *steps;
  guint n_steps;
  if (g_str_equal (language, "ja"))
  {
    steps = japanese_steps;
    n_steps = G_N_ELEMENTS (japanese_steps);
  }
  else if (g_str_equal (language, "ko"))
  {
    steps = korean_steps;
    n_steps = G_N_ELEMENTS (korean_steps);
  }
  else
  {
    g_printerr ("Expected --language ja or ko\n");
    return EXIT_FAILURE;
  }
  if (g_mkdir_with_parents (output, 0700) != 0)
    return EXIT_FAILURE;
  Display *display = XOpenDisplay (NULL);
  int event_base, error_base, major, minor;
  if (!display || !XTestQueryExtension (display, &event_base, &error_base, &major, &minor))
  {
    g_printerr ("An X11 display with XTEST is required\n");
    if (display) XCloseDisplay (display);
    return EXIT_FAILURE;
  }
  g_autoptr(GSubprocess) child = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                                 &error, probe_path, NULL);
  if (!child)
  {
    g_printerr ("Cannot start probe: %s\n", error->message);
    XCloseDisplay (display);
    return EXIT_FAILURE;
  }
  int fd = g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (g_subprocess_get_stdout_pipe (child)));
  g_autoptr(GIOChannel) channel = g_io_channel_unix_new (fd);
  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
  Probe probe = { .preedit = g_strdup (""), .text = g_strdup ("") };
  int status = EXIT_FAILURE;
  FILE *csv = NULL;
  g_autofree char *filename = NULL;
  g_autoptr(GPtrArray) latencies = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
  g_autoptr(GRand) random = g_rand_new_with_seed (20261108);
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (!probe.window && g_get_monotonic_time () < deadline)
  {
    if (!drain (channel, &probe)) goto cleanup;
    g_usleep (1000);
  }
  if (!probe.window) goto cleanup;
  XMoveWindow (display, probe.window, 40, 40);
  XRaiseWindow (display, probe.window);
  XSetInputFocus (display, probe.window, RevertToParent, CurrentTime);
  XWarpPointer (display, None, DefaultRootWindow (display), 0, 0, 0, 0, 900, 600);
  XSync (display, FALSE);
  g_usleep (300000);
  XWindowAttributes attributes;
  Window child_window;
  int x, y;
  if (!XGetWindowAttributes (display, probe.window, &attributes) ||
      !XTranslateCoordinates (display, probe.window, DefaultRootWindow (display),
                              0, 0, &x, &y, &child_window)) goto cleanup;
  if (probe.x < 0 || probe.y < 0 || probe.width <= 0 || probe.height <= 0 ||
      probe.x > attributes.width - probe.width ||
      probe.y > attributes.height - probe.height) goto cleanup;
  x += probe.x;
  y += probe.y;
  filename = g_build_filename (output, "samples.csv", NULL);
  csv = fopen (filename, "w");
  if (!csv) goto cleanup;
  fprintf (csv, "language,phase,cycle,step,injected_us,key_received_us,model_us,screen_lower_us,screen_upper_us,key_ms,model_ms,screen_lower_ms,screen_upper_ms,interval_ms,readback_ms,polls\n");
  for (guint step = 0; step < n_steps; step++)
    g_ptr_array_add (latencies, g_array_new (FALSE, FALSE, sizeof (Sample)));
  for (gint cycle = 0; cycle <= warmup + iterations; cycle++)
  {
    const char *phase = cycle == 0 ? "first" : cycle <= warmup ? "warmup" : "measured";
    for (guint step = 0; step < n_steps; step++)
    {
      /* Vary the phase relative to GTK's frame clock without busy waiting. */
      g_usleep (g_rand_int_range (random, 0, 20001));
      Sample sample = { 0 };
      if (!measure (display, channel, &probe, &steps[step], x, y,
                    probe.width, probe.height, poll_us, timeout_ms,
                    &sample, cycle == 0 ? output : NULL))
      {
        g_printerr ("Failed phase=%s cycle=%d step=%s\n", phase, cycle, steps[step].name);
        goto cleanup;
      }
      double upper = (sample.upper - sample.injected) / 1000.0;
      char key_ms[32] = "";
      if (sample.key)
        g_snprintf (key_ms, sizeof key_ms, "%.3f", (sample.key - sample.injected) / 1000.0);
      fprintf (csv, "%s,%s,%d,%s,%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT
                    ",%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%s,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n",
               language, phase, cycle, steps[step].name, sample.injected, sample.key, sample.model,
               sample.lower, sample.upper, key_ms,
               (sample.model - sample.injected) / 1000.0, (sample.lower - sample.injected) / 1000.0,
               upper, (sample.upper - sample.lower) / 1000.0, sample.readback / 1000.0, sample.polls);
      if (cycle > warmup)
        g_array_append_val (g_ptr_array_index (latencies, step), sample);
    }
  }
  if (fflush (csv) != 0 || ferror (csv)) goto cleanup;
  for (guint step = 0; step < n_steps; step++)
    print_summary (&steps[step], g_ptr_array_index (latencies, step));
  status = EXIT_SUCCESS;
cleanup:
  if (csv && fclose (csv) != 0) status = EXIT_FAILURE;
  g_free (probe.preedit);
  g_free (probe.text);
  g_subprocess_force_exit (child);
  g_subprocess_wait (child, NULL, NULL);
  XCloseDisplay (display);
  return status;
}
