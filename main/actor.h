#ifndef ACTOR_H
#define ACTOR_H
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <esp_log.h>

// Da der physische Relais-Aktor noch fehlt, verwenden wir die LED-Matrix
// als Statusanzeige für den Bewässerungszustand.
void actor_init(void);
void actor_set_relay(bool state);
void actor_start_timed_watering(int minutes);

#endif // ACTOR_H

