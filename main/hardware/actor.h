#ifndef ACTOR_H
#define ACTOR_H
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <esp_log.h>

void actor_init(void);
void actor_set_relay(bool off);

#endif // ACTOR_H

