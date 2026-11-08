#include <gtk/gtk.h>

static GtkWidget *entries[2];

static GtkWidget *
focus_widget (guint index)
{
#if GTK_MAJOR_VERSION >= 4
  return GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (entries[index])));
#else
  return entries[index];
#endif
}

static void
changed (GtkEditable *editable, gpointer data)
{
  (void) editable;
  (void) data;
#if GTK_MAJOR_VERSION >= 4
  const char *first = gtk_editable_get_text (GTK_EDITABLE (entries[0]));
  const char *second = gtk_editable_get_text (GTK_EDITABLE (entries[1]));
#else
  const char *first = gtk_entry_get_text (GTK_ENTRY (entries[0]));
  const char *second = gtk_entry_get_text (GTK_ENTRY (entries[1]));
#endif
  guint focused = gtk_widget_has_focus (focus_widget (0)) ? 1 :
                   gtk_widget_has_focus (focus_widget (1)) ? 2 : 0;
  gchar *state = g_strdup_printf ("%s\n%s\n%u\n", first, second, focused);
  GError *error = NULL;
  if (!g_file_set_contents (g_getenv ("NIMF_TEST_STATE"), state, -1, &error))
    g_error ("Cannot write test state: %s", error->message);
  g_print ("TEXT %s", state);
  g_free (state);
}

static void
focus_changed (GObject *object, GParamSpec *property, gpointer data)
{
  (void) object;
  (void) property;
  (void) data;
  changed (NULL, NULL);
}

static void
activate (GtkApplication *app, gpointer data)
{
  (void) data;
  GtkWidget *window = gtk_application_window_new (app);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
  gtk_window_set_title (GTK_WINDOW (window), "Nimf Mozc probe");
  gtk_window_set_default_size (GTK_WINDOW (window), 650, 220);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_widget_set_margin_top (box, 24);
#if GTK_MAJOR_VERSION >= 4
  if (g_getenv ("NIMF_TEST_CSD"))
    gtk_window_set_titlebar (GTK_WINDOW (window), gtk_header_bar_new ());
  gtk_window_set_child (GTK_WINDOW (window), box);
#else
  gtk_container_add (GTK_CONTAINER (window), box);
#endif
  for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
  {
    entries[i] = gtk_entry_new ();
#if GTK_MAJOR_VERSION >= 4
    gtk_box_append (GTK_BOX (box), entries[i]);
#else
    gtk_box_pack_start (GTK_BOX (box), entries[i], FALSE, FALSE, 0);
#endif
    g_signal_connect (entries[i], "changed", G_CALLBACK (changed), NULL);
    g_signal_connect (focus_widget (i), "notify::has-focus", G_CALLBACK (focus_changed), NULL);
  }
  changed (NULL, NULL);
#if GTK_MAJOR_VERSION < 4
  gtk_widget_show_all (window);
#endif
  gtk_window_present (GTK_WINDOW (window));
  gtk_widget_grab_focus (entries[0]);
}

int
main (int argc, char **argv)
{
  GtkApplication *app = gtk_application_new ("org.nimf.MozcProbe", G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  return status;
}
