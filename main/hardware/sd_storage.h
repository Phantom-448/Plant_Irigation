
#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include <stdbool.h>

void init_sd_card(void);
bool sd_card_ready(void);
bool sd_card_self_test(void);
void sd_write_log(const char* log_line);
void save_profile(const char* profil_name, const char* json_string);
void load_profile(const char* profil_name);
void sd_card_monitor_task(void *pvParameters);

#endif // SD_STORAGE_H