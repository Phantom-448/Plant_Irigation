#ifndef SD_STORAGE_H
#define SD_STORAGE_H

void init_sd_card(void);
void save_profile(const char* profil_name, const char* json_string);
void load_profile(const char* profil_name);
