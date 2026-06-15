#include "state.h"

SystemState_t sys_state = {
    .valve_1_state = false,
    .watering_duration = 10,
    .current_temp = 0.0f,
    .air_humidity = 0.0f,
    .soil_moisture_1 = 0,
    .active_profile = {
        .profile_name = "Tomate",
        .check_interval_minutes = 60,
        .base_watering_minutes = 8,
        .min_soil_moisture_percent = 50,
        .hot_temp_threshold = 28.0f,
        .hot_temp_extra_minutes = 4,
    }
};

SemaphoreHandle_t state_mutex = NULL;

void state_set_temperature(float temp) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        sys_state.current_temp = temp;
        xSemaphoreGive(state_mutex);
    }
}

void state_init(void) {
    state_mutex = xSemaphoreCreateMutex();
}