#include "temp.h"
#include "state.h"
#include "pin_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_random.h"

#if defined(GPIO_DHT22) || defined(GPIO_CHUMID)
#include "dht22.h"
#endif

#if defined(__has_include)
#  if __has_include("driver/temperature_sensor.h") && !defined(GPIO_DHT22) && (!defined(TEMP_SIMULATED) || (TEMP_SIMULATED == 0))
#    include "driver/temperature_sensor.h"
#    define HAVE_INTERNAL_TEMP 1
#  else
#    define HAVE_INTERNAL_TEMP 0
#  endif
#else
#  define HAVE_INTERNAL_TEMP 0
#endif

#ifdef TEMP_SIMULATED
#define USE_SIMULATED_TEMP (TEMP_SIMULATED)
#else
#define USE_SIMULATED_TEMP (0)
#endif

static const char *TAG = "TEMP_SENSOR";
static float filter_alpha = 0.1f;
static float filtered_temp = 0.0f;

float iir_filter_update(float current_value, float new_sample) {
    return (current_value * (1.0f - filter_alpha)) + (new_sample * filter_alpha);
}

void temp_sensor_init(void) {
    ESP_LOGI(TAG, "Initialisiere Temperatur-Sensor...");
    filtered_temp = 22.0f; // Standard-Startwert
}

float temp_sensor_read_filtered(void) {
    float new_sample = -1000.0f;

    if (USE_SIMULATED_TEMP) {
        uint32_t r = esp_random() & 0xFFFF;
        float jitter = (float)(r % 100) / 100.0f - 0.5f; 
        new_sample = filtered_temp + jitter * 0.2f; 
    }
#if defined(GPIO_DHT22)
    else {
        float temp_c = 0.0f, humidity = 0.0f;
        if (dht22_read((gpio_num_t)GPIO_DHT22, &temp_c, &humidity)) {
            new_sample = temp_c;
            
            // WICHTIG: Die Feuchtigkeit sofort mit abspeichern!
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sys_state.air_humidity = humidity;
                xSemaphoreGive(state_mutex);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read DHT22 sensor. Nutze letzten bekannten Wert.");
            new_sample = filtered_temp; // Fallback auf den alten Wert, kein Abbruch!
        }
    }
#else
    else {
        temperature_sensor_handle_t temp_sensor = NULL;
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
        if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
            if (temperature_sensor_enable(temp_sensor) == ESP_OK) {
                float raw_temp = 0.0f;
                if (temperature_sensor_get_celsius(temp_sensor, &raw_temp) == ESP_OK) {
                    new_sample = raw_temp;
                }
            }
            temperature_sensor_disable(temp_sensor);
            temperature_sensor_uninstall(temp_sensor);
        }
        if (new_sample < -500.0f) new_sample = filtered_temp;
    }
#endif
#if defined(GPIO_CHUMID)
    {
        float capacitive_humidity = 0.0f;
        if (c_humid_read((gpio_num_t)GPIO_CHUMID, &capacitive_humidity)) {
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sys_state.capacitive_humidity = capacitive_humidity;
                xSemaphoreGive(state_mutex);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read C_HUMID sensor. Nutze letzten bekannten Wert.");
        }
    }
#endif

    // Filter anwenden
    filtered_temp = iir_filter_update(filtered_temp, new_sample);

    // Temperatur in den State schreiben
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        sys_state.current_temp = filtered_temp;
        xSemaphoreGive(state_mutex);
    }

    return filtered_temp;
}
