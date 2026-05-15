#include "humid.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"
#include "state.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <stdlib.h>

static const char *TAG = "HUMID_SENSOR";

// Filter-Parameter gemäß Source
static float humid_filtered = 0.0f;
static const float humid_filter_alpha = 0.2f; // Gewichtung des neuen Wertes

/**
 * Hilfsfunktion: IIR-Filter Update
 */
static float humid_iir_filter(float old_val, float new_sample) {
    return (old_val * (1.0f - humid_filter_alpha)) + (new_sample * humid_filter_alpha);
}

void humid_sensor_init(void) {
    ESP_LOGI(TAG, "Initialisiere Feuchtigkeitssensor via I2C...");
    
    // Hinweis: I2C-Bus Konfiguration (GPIO 8/9 standard beim C3)
    // In einer realen Applikation wird hier der i2c_master_driver_install aufgerufen
}

float humid_sensor_read_filtered(void) {
    float raw_humidity = 0.0f;

    /* 
     * Hier erfolgt der I2C-Zugriff auf das Peripheriemodul.
     * Beispielhaft für einen Standard-Sensor:
     */
    // i2c_master_read_from_device(...); 
    
    // Simulation eines Sensorwerts (z.B. 45%):
    raw_humidity = 45.0f + ((float)rand() / (float)RAND_MAX * 2.0f);

    // Filterung anwenden, um Schwankungen zu minimieren[cite: 1]
    if (humid_filtered == 0.0f) {
        humid_filtered = raw_humidity; // Initialisierung
    } else {
        humid_filtered = humid_iir_filter(humid_filtered, raw_humidity);
    }

    // Thread-sicheres Schreiben in den globalen Systemstatus
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        sys_state.air_humidity = humid_filtered;
        xSemaphoreGive(state_mutex);
    }

    return humid_filtered;
}