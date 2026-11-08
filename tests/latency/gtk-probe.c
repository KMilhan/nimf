#include <gtk/gtk.h>
#include <stdio.h>
#if GTK_MAJOR_VERSION >= 4
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif

static void
report_text (const char *kind, const char *text)
{
  gint64 now = g_get_monotonic_time ();
  g_autofree char *encoded = g_base64_encode ((const guchar *) text, strlen (text));
  g_print ("%s\t%" G_GINT64_FORMAT "\t%s\n", kind, now, *encoded ? encoded : "-");
}

static void
preedit_changed (GtkWidget *widget, const char *text, gpointer data)
{
  (void) widget;
  (void) data;
  report_text ("preedit", text);
}

static void
text_changed (GtkEditable *editable, gpointer data)
{
  (void) data;
#if GTK_MAJOR_VERSION >= 4
  report_text ("text", gtk_editable_get_text (editable));
#else
  report_text ("text", gtk_entry_get_text (GTK_ENTRY (editable)));
#endif
}

#if GTK_MAJOR_VERSION >= 4
static gboolean
key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode,
             GdkModifierType state, gpointer data)
{
  (void) controller;
  (void) keycode;
  (void) state;
  (void) data;
  g_print ("key\t%" G_GINT64_FORMAT "\t%u\n", g_get_monotonic_time (), keyval);
  return FALSE;
}
#else
static gboolean
key_pressed (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  (void) widget;
  (void) data;
  g_print ("key\t%" G_GINT64_FORMAT "\t%u\n", g_get_monotonic_time (), event->keyval);
  return FALSE;
}
#endif

static gboolean
ready (gpointer data)
{
  GtkWidget *window = data;
  int x, y, width, height;
  unsigned long xid;
#if GTK_MAJOR_VERSION >= 4
  GtkWidget *entry = gtk_window_get_child (GTK_WINDOW (window));
  graphene_rect_t bounds;
  if (!gtk_widget_compute_bounds (entry, window, &bounds))
    return G_SOURCE_CONTINUE;
  x = bounds.origin.x;
  y = bounds.origin.y;
  width = bounds.size.width;
  height = bounds.size.height;
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
  /* GTK deprecated its X11 backend; this fixture specifically measures X11. */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  xid = gdk_x11_surface_get_xid (surface);
  G_GNUC_END_IGNORE_DEPRECATIONS
#else
  GtkWidget *entry = gtk_bin_get_child (GTK_BIN (window));
  if (!gtk_widget_translate_coordinates (entry, window, 0, 0, &x, &y))
    return G_SOURCE_CONTINUE;
  width = gtk_widget_get_allocated_width (entry);
  height = gtk_widget_get_allocated_height (entry);
  xid = gdk_x11_window_get_xid (gtk_widget_get_window (window));
#endif
  /* Exclude focus borders and adjacent candidate-window borders. */
  const int inset = 8;
  g_print ("ready\t%lu\t%d\t%d\t%d\t%d\n", xid, x + inset, y + inset,
           width - 2 * inset, height - 2 * inset);
  return G_SOURCE_REMOVE;
}

static void
activate (GtkApplication *app, gpointer data)
{
  (void) data;
  GtkSettings *settings = gtk_settings_get_default ();
  g_object_set (settings, "gtk-cursor-blink", FALSE,
                "gtk-enable-animations", FALSE, NULL);
  GtkWidget *window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Nimf latency probe");
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 100);
  GtkWidget *entry = gtk_entry_new ();
  gtk_widget_set_margin_start (entry, 16);
  gtk_widget_set_margin_end (entry, 16);
  gtk_widget_set_margin_top (entry, 16);
  gtk_widget_set_margin_bottom (entry, 16);
  if (g_getenv ("NIMF_LATENCY_HIDE_ENTRY"))
    gtk_widget_set_opacity (entry, 0);
#if GTK_MAJOR_VERSION >= 4
  gtk_window_set_child (GTK_WINDOW (window), entry);
  GtkWidget *text = GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (entry)));
  GtkEventController *controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), NULL);
  gtk_widget_add_controller (window, controller);
#else
  gtk_container_add (GTK_CONTAINER (window), entry);
  GtkWidget *text = entry;
  g_signal_connect (entry, "key-press-event", G_CALLBACK (key_pressed), NULL);
#endif
  g_signal_connect (text, "preedit-changed", G_CALLBACK (preedit_changed), NULL);
  g_signal_connect (entry, "changed", G_CALLBACK (text_changed), NULL);
#if GTK_MAJOR_VERSION < 4
  gtk_widget_show_all (window);
#endif
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (entry);
  g_timeout_add (250, ready, window);
}

int
main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  g_autoptr(GtkApplication) app = gtk_application_new ("org.nimf.LatencyProbe",
                                                      G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  return g_application_run (G_APPLICATION (app), argc, argv);
}
