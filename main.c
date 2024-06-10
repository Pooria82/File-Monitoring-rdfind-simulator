#include <gtk/gtk.h>
#include "duplicatefilechecker.h"

GtkWidget *log_view; // ویجت برای نمایش لاگ‌ها

static void on_browse_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog;
    GtkWidget *window = GTK_WIDGET(user_data);
    dialog = gtk_file_chooser_dialog_new("Open Directory", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        folder = gtk_file_chooser_get_filename(chooser);
        gtk_entry_set_text(GTK_ENTRY(user_data), folder);
        g_free(folder);
    }

    gtk_widget_destroy(dialog);
}

static void on_start_button_clicked(GtkButton *button, gpointer user_data) {
    const char *directory = gtk_entry_get_text(GTK_ENTRY(user_data));
    if (directory == NULL || strlen(directory) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Please select a directory.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    start_duplicate_file_checker(directory);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    gtk_text_buffer_set_text(buffer, "Duplicate file check completed. Log saved.\n", -1);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *entry;
    GtkWidget *browse_button;
    GtkWidget *start_button;
    GtkWidget *scrolled_window;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Duplicate File Checker");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    browse_button = gtk_button_new_with_label("Browse");
    g_signal_connect(browse_button, "clicked", G_CALLBACK(on_browse_button_clicked), entry);
    gtk_box_pack_start(GTK_BOX(vbox), browse_button, FALSE, FALSE, 0);

    start_button = gtk_button_new_with_label("Start");
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_button_clicked), entry);
    gtk_box_pack_start(GTK_BOX(vbox), start_button, FALSE, FALSE, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), log_view);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
