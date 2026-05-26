#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*button_pressed_callback_t)(void *arg);

bool button_init(gpio_num_t pin);
bool button_set_callback(button_pressed_callback_t callback, void *arg);
bool button_is_pressed(gpio_num_t pin);

#ifdef __cplusplus
}
#endif
