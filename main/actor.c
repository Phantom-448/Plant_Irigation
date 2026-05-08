// In actor.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <esp_log.h>

#define RELAY_GPIO 10

// Diese Funktion hast du schon
void actor_set_relay(bool state) {
    gpio_set_level(RELAY_GPIO, state ? 1 : 0);
}

// Interner Task, der nach der Zeit ausschaltet
void watering_timer_task(void *pvParameters) {
    int minutes = (int)pvParameters;
    
    ESP_LOGI("ACTOR", "Pumpe AN für %d Minuten", minutes);
    actor_set_relay(true);

    // Wartezeit in Millisekunden umrechnen
    // 1 Minute = 60 * 1000 ms
    vTaskDelay(pdMS_TO_TICKS(minutes * 60 * 1000));

    actor_set_relay(false);
    ESP_LOGI("ACTOR", "Pumpe automatisch AUS");
    
    vTaskDelete(NULL); // Task beenden
}

// Diese Funktion rufst du vom Webserver auf
void actor_start_timed_watering(int minutes) {
    // Starte einen Hintergrund-Task, damit der Webserver sofort antworten kann
    xTaskCreate(watering_timer_task, "water_task", 2048, (void*)minutes, 5, NULL);
}