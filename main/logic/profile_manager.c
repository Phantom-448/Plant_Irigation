#include "cJSON.h"
#include "state.h"
#include "profile_manager.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "PROFILE";
static ProfileList_t available_profiles = { .count = 0 };

void scan_profiles_on_sd(void) {
    ESP_LOGI("PROFILE", "Suche nach Profilen auf der SD-Karte...");
    
    // Ordner öffnen
    DIR *dir = opendir("/sdcard/profiles");
    if (!dir) {
        ESP_LOGE("PROFILE", "Konnte Profil-Ordner '/sdcard/profiles' nicht öffnen!");
        return;
    }

    available_profiles.count = 0;
    struct dirent *entry;
    
    // Alle Dateien im Ordner durchgehen
    while ((entry = readdir(dir)) != NULL) {
        // Prüfen, ob es eine normale Datei ist (DT_REG = Regular File)
        if (entry->d_type == DT_REG) {
            
            // Suchen, ob die Datei auf ".json" endet
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".json") == 0) {
                
                // Länge des Namens ohne ".json" berechnen
                int name_len = dot - entry->d_name;
                if (name_len >= PROFILE_NAME_LEN) {
                    name_len = PROFILE_NAME_LEN - 1; // Abschneiden, falls zu lang
                }

                // Name in unser Array kopieren und mit Nullterminator abschließen
                strncpy(available_profiles.names[available_profiles.count], entry->d_name, name_len);
                available_profiles.names[available_profiles.count][name_len] = '\0';

                ESP_LOGI("PROFILE", "Gefunden: %s", available_profiles.names[available_profiles.count]);
                
                available_profiles.count++;
                if (available_profiles.count >= MAX_PROFILES) {
                    ESP_LOGW("PROFILE", "Max. Anzahl an Profilen erreicht!");
                    break;
                }
            }
        }
    }
    closedir(dir);
    ESP_LOGI("PROFILE", "Scan abgeschlossen. %d Profile geladen.", available_profiles.count);
}

// Getter-Funktion für den Webserver
ProfileList_t* get_available_profiles(void) {
    return &available_profiles;
}

// Lädt ein Profil und aktiviert es
void load_and_activate_profile(const char* filename) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/profiles/%s.json", filename);

    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Profil '%s' nicht gefunden. Lade Standardwerte.", filename);
        return;
    }

    // Datei in den Speicher lesen
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* json_data = malloc(length + 1);
    if (json_data == NULL) {
        ESP_LOGE(TAG, "Speicher konnte nicht reserviert werden");
        fclose(f);
        return;
    }

    fread(json_data, 1, length, f);
    json_data[length] = '\0';
    fclose(f);

    // JSON parsen
    cJSON *root = cJSON_Parse(json_data);
    if (root) {
        // Mutex sperren, während wir das globale Profil überschreiben
        if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
            strncpy(sys_state.active_profile.profile_name, filename, PROFILE_NAME_LEN - 1);
            sys_state.active_profile.profile_name[PROFILE_NAME_LEN - 1] = '\0';

            // Werte aus JSON auslesen (mit Fallback-Werten, falls JSON fehlerhaft ist)
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
            ESP_LOGI(TAG, "Profil '%s' erfolgreich geladen und aktiviert!", filename);
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "Fehler beim Parsen des Profils '%s'", filename);
    }
    free(json_data);
}