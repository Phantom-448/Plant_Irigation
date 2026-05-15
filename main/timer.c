#include "timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "TIMER";
static TaskHandle_t s_timer_task = NULL;
static bool s_timer_running = false;
static int s_cycle_interval_minutes = 0;
static int s_watering_duration_minutes = 0;
static int64_t s_next_cycle_timestamp_us = 0;
static void (*s_cycle_callback)(void) = NULL;

static void timer_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Timer task gestartet: Intervall=%dmin, Dauer=%dmin",
             s_cycle_interval_minutes, s_watering_duration_minutes);

    while (s_timer_running) {
        int64_t now = esp_timer_get_time();
        s_next_cycle_timestamp_us = now + (int64_t)s_cycle_interval_minutes * 60 * 1000000LL;

        int64_t wait_us = s_next_cycle_timestamp_us - now;
        if (wait_us > 0) {
            uint32_t wait_ms = (uint32_t)((wait_us + 999) / 1000);
            vTaskDelay(pdMS_TO_TICKS(wait_ms));
        }

        if (!s_timer_running) {
            break;
        }

        ESP_LOGI(TAG, "Timer-Zyklus auslösen");
        if (s_cycle_callback) {
            s_cycle_callback();
        } else {
            ESP_LOGW(TAG, "Kein Callback registriert, Zyklus nicht ausgeführt");
        }

        if (s_watering_duration_minutes > 0) {
            ESP_LOGI(TAG, "Bewässerung läuft %d Minuten", s_watering_duration_minutes);
            vTaskDelay(pdMS_TO_TICKS((uint32_t)s_watering_duration_minutes * 60000));
        }
    }

    ESP_LOGI(TAG, "Timer task beendet");
    s_timer_task = NULL;
    vTaskDelete(NULL);
}

void timer_init(void)
{
    s_timer_running = false;
    s_timer_task = NULL;
    s_cycle_interval_minutes = 0;
    s_watering_duration_minutes = 0;
    s_next_cycle_timestamp_us = 0;
    s_cycle_callback = NULL;
}

bool timer_start_cycle(int cycle_interval_minutes, int watering_duration_minutes)
{
    if (cycle_interval_minutes <= 0 || watering_duration_minutes < 0) {
        ESP_LOGE(TAG, "Ungültige Timerwerte: interval=%d, duration=%d",
                 cycle_interval_minutes, watering_duration_minutes);
        return false;
    }

    if (s_timer_running) {
        ESP_LOGW(TAG, "Timer läuft bereits, starte neu");
        timer_stop();
    }

    s_cycle_interval_minutes = cycle_interval_minutes;
    s_watering_duration_minutes = watering_duration_minutes;
    s_timer_running = true;

    BaseType_t result = xTaskCreate(timer_task, "timer_task", 3072, NULL, 5, &s_timer_task);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Timer task konnte nicht gestartet werden");
        s_timer_running = false;
        return false;
    }

    return true;
}

void timer_stop(void)
{
    if (!s_timer_running) {
        return;
    }

    s_timer_running = false;
    s_next_cycle_timestamp_us = 0;
    if (s_timer_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool timer_is_running(void)
{
    return s_timer_running;
}

int timer_get_seconds_to_next_cycle(void)
{
    if (!s_timer_running || s_next_cycle_timestamp_us <= 0) {
        return 0;
    }
    int64_t now = esp_timer_get_time();
    int64_t diff_us = s_next_cycle_timestamp_us - now;
    return diff_us > 0 ? (int)(diff_us / 1000000LL) : 0;
}

int timer_get_cycle_interval_minutes(void)
{
    return s_cycle_interval_minutes;
}

int timer_get_watering_duration_minutes(void)
{
    return s_watering_duration_minutes;
}

void timer_register_callback(void (*callback)(void))
{
    s_cycle_callback = callback;
}
