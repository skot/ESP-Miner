#ifndef DISPLAY_H_
#define DISPLAY_H_

esp_err_t display_init(void);
bool display_active(void);
void display_clear();
void display_show_status(const char *messages[], size_t message_count);
void display_show_logo();

#endif /* DISPLAY_H_ */