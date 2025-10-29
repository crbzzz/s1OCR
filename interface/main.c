#include <gtk/gtk.h>

#include "ocr_window.h"

static void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    OcrAppWindow *window = ocr_app_window_new(app);
    if (window == NULL) 
    {
        g_critical("Impossible de créer la fenêtre principale.");
        return;
    }

    gtk_widget_show_all(GTK_WIDGET(window));
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("com.ocr.wordsearch", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
