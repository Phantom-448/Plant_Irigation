#include "temp.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEMP_SENSOR";

// Handle für den ESP-internen Sensor
temperature_sensor_handle_t temp_sensor = NULL;

// Einfacher IIR-Filter Koeffizient (Alpha)
// Ein kleiner Wert (z.B. 0.1) glättet stark, reagiert aber langsam.
static float filter_alpha = 0.1f;
static float filtered_temp = 0.0f;

/**
 * Filter-Logik basierend auf:
 * Ein IIR-Filter berechnet den neuen Wert aus dem alten gefilterten Wert 
 * und dem neuen Messwert.
 */
float iir_filter_update(float current_value, float new_sample) {
    return (current_value * (1.0f - filter_alpha)) + (new_sample * filter_alpha);
}

void temp_sensor_init(void) {
    ESP_LOGI(TAG, "Initialisiere internen Temperatur-Sensor...");

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // Ersten Wert lesen, um Filter zu initialisieren
    float first_val;
    temperature_sensor_get_celsius(temp_sensor, &first_val);
    filtered_temp = first_val;
}

float temp_sensor_read_filtered(void) {
    float raw_temp;
    if (temperature_sensor_get_celsius(temp_sensor, &raw_temp) == ESP_OK) {
        // Anwendung des IIR-Filters
        filtered_temp = iir_filter_update(filtered_temp, raw_temp);
        
        // Wert in den globalen Systemstatus schreiben (Thread-safe)
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sys_state.current_temp = filtered_temp;
            xSemaphoreGive(state_mutex);
        }
        return filtered_temp;
    }
    return -1.0f; // Fehlerfall
}