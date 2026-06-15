#include "profile_manager.h"
#include "cJSON.h"
#include "state.h"
#include "smart_logic.h" // <-- Behebt den "implicit declaration" Fehler
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

// <-- Behebt die Warnung, da TAG jetzt in den ESP_LOGs genutzt wird
static const char *TAG = "PROFILE"; 
static ProfileList_t available_profiles = { .count = 0 };

void scan_profiles_on_sd(void) {
    ESP_LOGI(TAG, "Suche nach Pflanzenprofilen auf der SD-Karte...");
    
    DIR *dir = opendir("/sdcard/profiles");
    if (!dir) {
        ESP_LOGE(TAG, "Konnte Profil-Ordner '/sdcard/profiles' nicht oeffnen!");
        return;
    }

    available_profiles.count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".json") == 0) {
                
                int name_len = dot - entry->d_name;
                if (name_len >= PROFILE_NAME_LEN) {
                    name_len = PROFILE_NAME_LEN - 1; 
                }

                strncpy(available_profiles.names[available_profiles.count], entry->d_name, name_len);
                available_profiles.names[available_profiles.count][name_len] = '\0';

                ESP_LOGI(TAG, "Pflanze gefunden: %s", available_profiles.names[available_profiles.count]);
                
                available_profiles.count++;
                if (available_profiles.count >= MAX_PROFILES) {
                    ESP_LOGW(TAG, "Max. Anzahl an Pflanzenprofilen erreicht!");
                    break;
                }
            }
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Scan abgeschlossen. %d Pflanzenprofile geladen.", available_profiles.count);
}

ProfileList_t* get_available_profiles(void) {
    return &available_profiles;
}

void load_and_activate_profile(const char* filename) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/profiles/%s.json", filename);

    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Pflanzenprofil '%s' nicht gefunden. Behalte aktuelle Werte.", filename);
        return;
    }

    // --- HIER FEHLTE DIR VORHER DER CODE ---
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Die Variable wird hier deklariert
    char* json_data = malloc(length + 1);
    if (json_data == NULL) {
        ESP_LOGE(TAG, "Speicher konnte nicht reserviert werden");
        fclose(f);
        return;
    }

    fread(json_data, 1, length, f);
    json_data[length] = '\0';
    fclose(f);
    // --------------------------------------

    cJSON *root = cJSON_Parse(json_data);
    if (root) {
        if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
            strncpy(sys_state.active_profile.profile_name, filename, PROFILE_NAME_LEN - 1);
            sys_state.active_profile.profile_name[PROFILE_NAME_LEN - 1] = '\0';

            cJSON *item = cJSON_GetObjectItem(root, "check_interval_minutes");
            sys_state.active_profile.check_interval_minutes = item ? item->valueint : 60;

            item = cJSON_GetObjectItem(root, "base_watering_minutes");
            sys_state.active_profile.base_watering_minutes = item ? item->valueint : 5;

            item = cJSON_GetObjectItem(root, "min_soil_moisture_percent");
            sys_state.active_profile.min_soil_moisture_percent = item ? item->valueint : 40;

            item = cJSON_GetObjectItem(root, "hot_temp_threshold");
            sys_state.active_profile.hot_temp_threshold = item ? item->valuedouble : 30.0;

            item = cJSON_GetObjectItem(root, "hot_temp_extra_minutes");
            sys_state.active_profile.hot_temp_extra_minutes = item ? item->valueint : 3;

            xSemaphoreGive(state_mutex);
            ESP_LOGI(TAG, "Pflanzenprofil '%s' erfolgreich geladen. Starte Re-Evaluierung.", filename);
            
            // Triggert sofort die smarte Logik und den neuen Timer!
            smart_logic_evaluate(); 
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "Fehler beim Parsen der JSON fuer Pflanze '%s'", filename);
    }
    
    free(json_data); // Speicher wieder freigeben
}