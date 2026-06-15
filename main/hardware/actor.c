#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pin_config.h"
#include "state.h"
#include <esp_log.h>
#include <stdbool.h>

static bool s_actor_state = false;
static TaskHandle_t s_watering_task = NULL;
static volatile bool s_cancel_watering = false; // Neuer Abbruch-Flag

void actor_set_relay(bool on) {
    s_actor_state = on;
    gpio_set_level(GPIO_RELAY, on ? 1 : 0);

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        sys_state.valve_1_state = on;
        xSemaphoreGive(state_mutex);
        ESP_LOGI("ACTOR", "Relais %s", on ? "EIN" : "AUS");
    }
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

static void watering_task(void *pvParameters) {
    int minutes = (int)(intptr_t)pvParameters;
    ESP_LOGI("ACTOR", "Starte Bewässerung für %d Minuten", minutes);
    
    actor_set_relay(true);
    
    if (minutes > 0) {
        uint32_t remaining_minutes = (uint32_t)minutes;
        while (remaining_minutes > 0 && !s_cancel_watering) {
            uint32_t chunk_minutes = (remaining_minutes > 5) ? 5 : remaining_minutes;
            vTaskDelay(pdMS_TO_TICKS(chunk_minutes * 60000));
            if (s_cancel_watering) break;
            remaining_minutes -= chunk_minutes;
        }
    }
    
    actor_set_relay(false);
    s_watering_task = NULL;
    vTaskDelete(NULL);
}

void actor_start_timed_watering(int minutes) {
    if (minutes <= 0) return;

    if (s_watering_task != NULL) {
        ESP_LOGI("ACTOR", "Bestehende Bewässerung wird abgebrochen.");
        s_cancel_watering = true;
        xTaskAbortDelay(s_watering_task); // Task wecken
        while(s_watering_task != NULL) { vTaskDelay(pdMS_TO_TICKS(10)); } // Warten bis beendet
    }

    s_cancel_watering = false;
    xTaskCreate(watering_task, "watering_task", 3072, (void *)(intptr_t)minutes, 5, &s_watering_task);
}