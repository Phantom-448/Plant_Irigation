#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

bool dht22_read(gpio_num_t pin, float *out_temp_c, float *out_humidity);

#ifdef __cplusplus
}
#endif
