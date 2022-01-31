#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

//#pragma comment(lib,"ole32.lib")
//#pragma comment(lib,"Comdlg32.lib")

char *ui_get_mmf_directory (void) {
	GtkWidget *folder_open;
	char *path_a, *path_b;
	folder_open = gtk_file_chooser_dialog_new ("Browse for folder...", NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	path_b = NULL;
	if (gtk_dialog_run (GTK_DIALOG (folder_open)) == GTK_RESPONSE_ACCEPT) {
		path_a = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (folder_open));

		if (path_a != NULL) {
			path_b = strdup (path_a);

			g_free (path_a);
		}
	}
	
	gtk_widget_destroy (folder_open);
	return path_b;
}

void ui_init (int *argc, char ***argv) {
	gtk_init (argc, argv);
}

void ui_show_message_error (const char *message) {
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	
	gtk_widget_destroy (dialog);
}

void ui_show_message_warning (const char *message) {
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", message);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	
	gtk_widget_destroy (dialog);
}

void ui_show_message_info (const char *message) {
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", message);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	
	gtk_widget_destroy (dialog);
}

char *ui_save_file (void) {
	GtkWidget *folder_open;
	char *path_a, *path_b;
	GtkFileFilter *filter;
	
	folder_open = gtk_file_chooser_dialog_new ("Save obj file...", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (folder_open), TRUE);
	
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "Wavefront OBJ file (*.obj)");
	gtk_file_filter_add_pattern (filter, "*.obj");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (folder_open), filter);
	
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (folder_open), filter);
	
	path_b = NULL;
	if (gtk_dialog_run (GTK_DIALOG (folder_open)) == GTK_RESPONSE_ACCEPT) {
		path_a = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (folder_open));

		if (path_a != NULL) {
			path_b = strdup (path_a);

			g_free (path_a);
		}
	}
	
	gtk_widget_destroy (folder_open);
	return path_b;
}

