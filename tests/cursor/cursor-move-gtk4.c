/* Checks that the GTK4 Nimf module re-reports the cursor location after the
 * application window moves. The caller counts SET_CURSOR_LOCATION messages in
 * the module log, so this program only drives the toolkit. The move goes
 * through X directly because the test session runs without a window manager. */

#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>

static void
pump (void)
{
    for (int i = 0; i < 50; i++) {
        while (g_main_context_iteration (NULL, FALSE))
            ;
        g_usleep (20000);
    }
}

int
main (void)
{
    GdkRectangle area = {10, 20, 0, 20};

    gtk_init ();

    GtkWidget *window = gtk_window_new ();
    gtk_window_set_default_size (GTK_WINDOW (window), 320, 200);
    gtk_widget_set_visible (window, TRUE);
    pump ();

    GtkIMContext *context = gtk_im_multicontext_new ();
    gtk_im_context_set_client_widget (context, window);
    /* Focus creates the daemon side context, so later reports can arrive. */
    gtk_im_context_focus_in (context);
    pump ();

    /* Same area twice: the second report must not add a message. */
    gtk_im_context_set_cursor_location (context, &area);
    gtk_im_context_set_cursor_location (context, &area);
    pump ();

    GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
    XMoveWindow (gdk_x11_display_get_xdisplay (gdk_display_get_default ()),
                 gdk_x11_surface_get_xid (surface), 200, 200);
    pump ();

    /* The window moved, so the same area is a new location on screen. */
    gtk_im_context_set_cursor_location (context, &area);
    gtk_im_context_set_cursor_location (context, &area);
    pump ();

    g_object_unref (context);
    gtk_window_destroy (GTK_WINDOW (window));

    return 0;
}
