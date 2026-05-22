// --- logger.c ---
#include "logger.h"
#include "state.h"
#include <time.h>
#include <stdio.h>
#include "esp_log.h"
#include "sd_storage.h"

void logger_write_sensor_data(void) {
    // 1. Zeitstempel generieren
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char log_buffer[128];
    
    // 2. Daten sicher aus dem State holen und als CSV formatieren
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(log_buffer, sizeof(log_buffer), "%s,%.1f,%.1f,%d,%d",
                 time_str,
                 sys_state.current_temp,
                 sys_state.air_humidity,
                 sys_state.soil_moisture_1,
                 sys_state.valve_1_state ? 1 : 0);
                 
        xSemaphoreGive(state_mutex);
        
        // 3. An den "Archivar" (SD-Modul) übergeben
        sd_write_log(log_buffer);
        ESP_LOGI("LOGGER", "Eintrag gespeichert: %s", log_buffer);
    } else {
        ESP_LOGW("LOGGER", "Daten-Mutex blockiert, Log übersprungen.");
    }
}