#include "air_sensor.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "AIR_SENSOR";
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static int64_t wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us){
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((uint32_t)(esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return esp_timer_get_time() - start;
}

static bool air_sensor_read_raw(gpio_num_t pin, uint8_t data[5]){
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); 
    
    gpio_set_level(pin, 1);

    int64_t start = esp_timer_get_time();
    while ((uint32_t)(esp_timer_get_time() - start) < 40) {
        ;
    }

    gpio_set_direction(pin, GPIO_MODE_INPUT);

    portENTER_CRITICAL(&mux);

    if (wait_for_level(pin, 0, 1000) < 0) {
        portEXIT_CRITICAL(&mux);
        return false;
    }
    if (wait_for_level(pin, 1, 1000) < 0) {
        portEXIT_CRITICAL(&mux);
        return false;
    }

    for (int i = 0; i < 40; ++i) {
        if (wait_for_level(pin, 0, 1000) < 0) {
            portEXIT_CRITICAL(&mux);
            return false;
        }
        if (wait_for_level(pin, 1, 1000) < 0) {
            portEXIT_CRITICAL(&mux);
            return false;
        }

        int64_t t_high_start = esp_timer_get_time();
        while (gpio_get_level(pin) == 1) {
            if ((uint32_t)(esp_timer_get_time() - t_high_start) > 200) {
                break;
            }
        }
        int64_t t_high_duration = esp_timer_get_time() - t_high_start;

        int bit_index = i / 8;
        data[bit_index] <<= 1;
        
        if (t_high_duration > 40) {
            data[bit_index] |= 1;
        }
    }

    portEXIT_CRITICAL(&mux);

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGD(TAG, "Checksum-Fehler: Berechnet %02X, Erwartet %02X", checksum, data[4]);
        return false;
    }

    return true;
}

bool air_sensor_read(gpio_num_t pin, float *out_temp_c, float *out_humidity){
    uint8_t data[5] = {0};
    int max_retries = 3;
    
    for (int retry = 0; retry < max_retries; retry++) {
        if (air_sensor_read_raw(pin, data)) {
            *out_humidity = (float)data[0] + ((float)data[1] / 10.0f);
            *out_temp_c = (float)data[2] + ((float)data[3] / 10.0f);
            return true;
        }
        
        if (retry < max_retries - 1) {
            ESP_LOGI(TAG, "WLAN-Interferenz erkannt. Lese Sensor in 2s neu aus (Versuch %d/3)...", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2100)); 
        }
    }

    ESP_LOGW(TAG, "Konnte DHT11 trotz 3 Versuchen nicht stabil auslesen.");
    return false;
}

bool c_humid_read(gpio_num_t pin, float *out_humidity){
    uint8_t data[5] = {0};
    if (!air_sensor_read_raw(pin, data)) {
        ESP_LOGW(TAG, "Bus-Fehler an GPIO %d", pin);
        return false;
    }

    *out_humidity = ((data[0] << 8) | data[1]) / 10.0f;
    return true;
}