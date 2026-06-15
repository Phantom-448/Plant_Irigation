#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Liest Temperatur und Luftfeuchtigkeit vom DHT11 Sensor
bool air_sensor_read(gpio_num_t pin, float *out_temp_c, float *out_humidity);

// Spezifisch für deinen kapazitiven Sensor, falls dieser über dasselbe Protokoll läuft
bool c_humid_read(gpio_num_t pin, float *out_humidity);

#ifdef __cplusplus
}
#endif