#include "logger.h"
#include "state.h"
#include "pin_config.h"
#include <time.h>
#include <stdio.h>
#include "esp_log.h"
#include "sd_storage.h"

void logger_write_sensor_data(void) {
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char log_buffer[128];
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        snprintf(log_buffer, sizeof(log_buffer), "%s,%.1f,%.1f,%d,%d\n",
                 time_str,
                 sys_state.current_temp,
                 sys_state.air_humidity,
                 sys_state.soil_moisture_1,
                 sys_state.valve_1_state ? 1 : 0);
                 
        xSemaphoreGive(state_mutex);
        
        
        sd_write_log(log_buffer);
        ESP_LOGI("LOGGER", "Eintrag gespeichert: %s", log_buffer);
    } else {
        ESP_LOGW("LOGGER", "Daten-Mutex blockiert, Log übersprungen.");
    }
}