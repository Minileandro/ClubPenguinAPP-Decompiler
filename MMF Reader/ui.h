#ifndef __UI_H__
#define __UI_H__

void ui_init (int *argc, char ***argv);
char *ui_get_mmf_directory (void);
void ui_show_message_error (const char *message);
void ui_show_message_warning (const char *message);
void ui_show_message_info (const char *message);
char *ui_save_file (void);

#endif /* __UI_H__ */
