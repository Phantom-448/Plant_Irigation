#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*button_pressed_callback_t)(void *arg);

bool button_init(gpio_num_t pin);
void button_set_callback(button_pressed_callback_t cb, void *arg);


#ifdef __cplusplus
}
#endif
