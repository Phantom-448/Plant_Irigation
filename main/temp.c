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

#ifdef GPIO_DHT22
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
    float new_sample = filtered_temp;

#if USE_SIMULATED_TEMP
    if (USE_SIMULATED_TEMP) {
        float jitter = ((float)(esp_random() % 100) / 100.0f) - 0.5f;
        new_sample = filtered_temp + jitter * 0.2f;
    }
#endif

#ifdef GPIO_DHT22
    // Block für den echten DHT22
    float temp_c = 0.0f, humidity = 0.0f;
    if (dht22_read((gpio_num_t)GPIO_DHT22, &temp_c, &humidity)) {
        new_sample = temp_c;
        
        // Optimierung: Luftfeuchtigkeit sofort in den State schreiben!
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sys_state.air_humidity = humidity;
            xSemaphoreGive(state_mutex);
        }
    } else {
        ESP_LOGW(TAG, "Failed to read DHT22 sensor");
        return filtered_temp; // Fail-Safe
    }
#elif HAVE_INTERNAL_TEMP
    // Block für den internen ESP-Sensor (Fallback)
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
#endif

    if (new_sample < -500.0f) return -500.0f; // Dummy check

    filtered_temp = iir_filter_update(filtered_temp, new_sample);
    return filtered_temp;
}