#include "state.h"

// Hier werden die Variablen WIRKLICH im Speicher angelegt
SystemState_t sys_state = {
    .valve_1_state = false,
    .valve_2_state = false,
    .watering_duration = 10,
    .current_temp = 0.0f,
    .air_humidity = 0.0f
};

SemaphoreHandle_t state_mutex = NULL;

void state_init(void) {
    state_mutex = xSemaphoreCreateMutex();
}