#include "air_sensor.h"
#include "esp_log.h"
#include "dht.h"

static const char *TAG = "AIR_SENSOR";


static const dht_sensor_type_t SENSOR_TYPE = DHT_TYPE_DHT11; 

bool air_sensor_read(gpio_num_t pin, float *out_temp_c, float *out_humidity) {
    int16_t temperature = 0;
    int16_t humidity = 0;
    
    
    esp_err_t res = dht_read_data(SENSOR_TYPE, pin, &humidity, &temperature);
    
    if (res == ESP_OK) {
    
        *out_temp_c = temperature / 10.0f;
        *out_humidity = humidity / 10.0f;
        return true;
    } else {
        ESP_LOGW(TAG, "Hardware-Lesezyklus fehlgeschlagen: %s", esp_err_to_name(res));
        return false;
    }
}

bool c_humid_read(gpio_num_t pin, float *out_humidity) {
    int16_t temperature = 0;
    int16_t humidity = 0;
    
    if (dht_read_data(SENSOR_TYPE, pin, &humidity, &temperature) == ESP_OK) {
        *out_humidity = humidity / 10.0f;
        return true;
    }
    
    ESP_LOGW(TAG, "Bus-Fehler an GPIO %d", pin);
    return false;
}