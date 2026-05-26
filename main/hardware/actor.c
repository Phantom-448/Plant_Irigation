// In actor.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pin_config.h"
#include <esp_log.h>
#include <stdbool.h>

static bool s_actor_state = false;
static TaskHandle_t s_watering_task = NULL;

static void watering_task(void *pvParameters)
{
    int minutes = (int)(intptr_t)pvParameters;
    ESP_LOGI("ACTOR", "Starte manuelle Bewässerung für %d Minuten", minutes);
    actor_set_relay(true);
    if (minutes > 0) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)minutes * 60000));
    }
    actor_set_relay(false);
    s_watering_task = NULL;
    vTaskDelete(NULL);
}

void actor_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    actor_set_relay(false);
}

void actor_set_relay(bool off) {
    s_actor_state = off;
    gpio_set_level(GPIO_RELAY, off ? 1 : 0);
    ESP_LOGI("ACTOR", "Relais %s", off ? "EIN" : "AUS");
}

void actor_start_timed_watering(int minutes)
{
    if (minutes <= 0) {
        ESP_LOGW("ACTOR", "Ungültige Bewässerungsdauer: %d Minuten", minutes);
        return;
    }

    if (s_watering_task != NULL) {
        ESP_LOGW("ACTOR", "Bestehende Bewässerung abbrechen und neu starten");
        actor_set_relay(false);
        vTaskDelete(s_watering_task);
        s_watering_task = NULL;
    }

    BaseType_t result = xTaskCreate(watering_task,
                                    "watering_task",
                                    3072,
                                    (void *)(intptr_t)minutes,
                                    5,
                                    &s_watering_task);
    if (result != pdPASS) {
        ESP_LOGE("ACTOR", "Bewässerungs-Task konnte nicht gestartet werden");
        actor_set_relay(false);
        s_watering_task = NULL;
    }
}
