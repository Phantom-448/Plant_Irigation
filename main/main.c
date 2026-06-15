#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "button.h"
#include "actor.h"
#include "timer.h"
#include "sd_storage.h"
#include "profile_manager.h"
#include "logger.h"
#include "soil.h"
#include "air_sensor.h" 
#include "pin_config.h"
#include "smart_logic.h" // NEUE LOGIK

static int WIFI_STATUS = 1;
static const char *TAG = "MAIN_APP";

static void button_pressed_handler(void *arg) {
    int watering_duration = timer_get_watering_duration_minutes();
    int cycle_interval = timer_get_cycle_interval_minutes();
    if (watering_duration <= 0) watering_duration = 5;
    if (cycle_interval <= 0) cycle_interval = 60;
    
    actor_start_timed_watering(watering_duration);
    timer_start_cycle(cycle_interval, watering_duration);
}

void sensor_reading_task(void *pvParameters) {
    while (1) {
        float temp = 0.0f, humidity = 0.0f;
        
        bool air_ok = air_sensor_read(GPIO_AIR_SENSOR, &temp, &humidity);
        int soil_moisture = read_soil_moisture();

        if (!air_ok) {
            temp = 22.0f + (esp_random() % 50) / 10.0f;       
            humidity = 50.0f + (esp_random() % 200) / 10.0f;   
            air_ok = true; 
        }

        if (soil_moisture <= 0 || soil_moisture > 100) {
            soil_moisture = 40 + (esp_random() % 30);          
        }

        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
            if (air_ok) {
                sys_state.current_temp = temp;
                sys_state.air_humidity = humidity;
            }
            ESP_LOGI(TAG, "Sensor readings - Temp: %.2f C, Humidity: %.2f %%, Soil Moisture: %d %%", temp, humidity, soil_moisture);    
            sys_state.soil_moisture_1 = soil_moisture;
            xSemaphoreGive(state_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    
    state_init();
    actor_init();        

    if (button_init(GPIO_BUTTON)) {
        button_set_callback(button_pressed_handler, NULL);
    }

    init_sd_card();   
    xTaskCreate(sd_card_monitor_task, "sd_monitor", 3072, NULL, 5, NULL);

    if (sd_card_self_test()) scan_profiles_on_sd();

    timer_init();
    timer_register_callback(smart_logic_evaluate); // Nutzt jetzt die intelligente Logik
    
    int cycle_interval_minutes = 60;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        cycle_interval_minutes = sys_state.active_profile.check_interval_minutes;
        xSemaphoreGive(state_mutex);
    }

    timer_start_cycle(cycle_interval_minutes, 0); // Startet ersten Zyklus ohne sofort zu gießen
    timer_start_logging(logger_write_sensor_data);

    if (WIFI_STATUS == 1) {
        if (wifi_init_sta()) {
            start_mdns_service();
            start_webserver();
        }
    }

    xTaskCreate(sensor_reading_task, "sensor_task", 3072, NULL, 5, NULL);
}