/* Checks that the GTK3 Nimf module re-reports the cursor location after the
 * application window moves. The caller counts SET_CURSOR_LOCATION messages in
 * the module log, so this program only drives the toolkit. */

#include <gtk/gtk.h>

static void
pump (void)
{
    for (int i = 0; i < 50; i++) {
        while (gtk_events_pending ())
            gtk_main_iteration ();
        g_usleep (20000);
    }
}

int
main (int argc, char **argv)
{
    GdkRectangle area = {10, 20, 0, 20};

    gtk_init (&argc, &argv);

    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 320, 200);
    gtk_widget_show_all (window);
    pump ();

    /* The multicontext loads the module selected by GTK_IM_MODULE, which is the
     * path a real text widget uses. */
    GtkIMContext *context = gtk_im_multicontext_new ();
    gtk_im_context_set_client_window (context, gtk_widget_get_window (window));
    /* Focus creates the daemon side context, so later reports can arrive. */
    gtk_im_context_focus_in (context);
    pump ();

    /* Same area twice: the second report must not add a message. */
    gtk_im_context_set_cursor_location (context, &area);
    gtk_im_context_set_cursor_location (context, &area);
    pump ();

    gtk_window_move (GTK_WINDOW (window), 200, 200);
    pump ();

    /* The window moved, so the same area is a new location on screen. */
    gtk_im_context_set_cursor_location (context, &area);
    gtk_im_context_set_cursor_location (context, &area);
    pump ();

    g_object_unref (context);
    gtk_widget_destroy (window);

    return 0;
}
