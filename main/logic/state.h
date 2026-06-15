#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define PROFILE_NAME_LEN 32

typedef struct {
    char profile_name[PROFILE_NAME_LEN];
    int check_interval_minutes;
    int base_watering_minutes;
    int min_soil_moisture_percent;
    float hot_temp_threshold;
    int hot_temp_extra_minutes;
} WateringProfile_t;

typedef struct {
    bool valve_1_state;
    int watering_duration;
    float current_temp;
    float air_humidity;
    int soil_moisture_1;
    WateringProfile_t active_profile;
} SystemState_t;

extern SystemState_t sys_state;
extern SemaphoreHandle_t state_mutex;

void state_init(void);
void state_set_temperature(float temp);