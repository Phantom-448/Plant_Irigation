#include "logger.h"
#include "state.h"
#include "pin_config.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "sd_storage.h"

static const char *TAG = "LOGGER";

void logger_write_sensor_data(void) {
    time_t now;
    time(&now);
    char time_str[64];
    
    
    if (now < 100000) { 
        snprintf(time_str, sizeof(time_str), "Uptime_%llds", esp_timer_get_time() / 1000000LL);
    } else {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

    
    if (access(SD_LOG_FILE, F_OK) != 0) {
        ESP_LOGI(TAG, "Logdatei existiert nicht. Erstelle CSV-Header.");
        sd_write_log("Zeitstempel,Temperatur_C,Luftfeuchte_Prozent,Bodenfeuchte_Prozent,Pumpe_Aktiv");
    }

    char log_buffer[128];
    
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        snprintf(log_buffer, sizeof(log_buffer), "%s,%.1f,%.1f,%d,%d",
                 time_str, 
                 sys_state.current_temp, 
                 sys_state.air_humidity,
                 sys_state.soil_moisture_1, 
                 sys_state.valve_1_state ? 1 : 0);
                 
        xSemaphoreGive(state_mutex);
        
        sd_write_log(log_buffer);
        ESP_LOGI(TAG, "Eintrag gespeichert: %s", log_buffer);
    } else {
        ESP_LOGW(TAG, "Daten-Mutex blockiert, Log übersprungen.");
    }
}