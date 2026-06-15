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

// Spinlock für sicheren 64-Bit Zugriff
static portMUX_TYPE timer_mux = portMUX_INITIALIZER_UNLOCKED;

static void timer_task(void *pvParameters) {
    while (s_timer_running) {
        int64_t now = esp_timer_get_time();
        
        portENTER_CRITICAL(&timer_mux);
        s_next_cycle_timestamp_us = now + (int64_t)s_cycle_interval_minutes * 60 * 1000000LL;
        int64_t target = s_next_cycle_timestamp_us;
        portEXIT_CRITICAL(&timer_mux);

        int64_t wait_us = target - now;
        if (wait_us > 0) {
            uint32_t wait_ms = (uint32_t)((wait_us + 999) / 1000);
            vTaskDelay(pdMS_TO_TICKS(wait_ms)); // Kann durch xTaskAbortDelay geweckt werden
        }

        if (!s_timer_running) break;

        if (s_cycle_callback) {
            s_cycle_callback();
        }
    }
    s_timer_task = NULL;
    vTaskDelete(NULL);
}

void timer_init(void) {
    s_timer_running = false;
    s_timer_task = NULL;
}

bool timer_start_cycle(int cycle_interval_minutes, int watering_duration_minutes) {
    if (cycle_interval_minutes <= 0 || watering_duration_minutes < 0) {
        return false;
    }

    if (s_timer_running) {
        timer_stop();
    }

    s_cycle_interval_minutes = cycle_interval_minutes;
    s_watering_duration_minutes = watering_duration_minutes;
    s_timer_running = true;

    xTaskCreate(timer_task, "timer_task", 3072, NULL, 5, &s_timer_task);
    return true;
}

void timer_stop(void) {
    if (!s_timer_running) return;
    s_timer_running = false;
    if (s_timer_task) {
        xTaskAbortDelay(s_timer_task); // Task sofort aus dem Schlaf wecken
    }
}

int timer_get_seconds_to_next_cycle(void) {
    if (!s_timer_running) return 0;
    
    portENTER_CRITICAL(&timer_mux);
    int64_t target = s_next_cycle_timestamp_us;
    portEXIT_CRITICAL(&timer_mux);
    
    int64_t now = esp_timer_get_time();
    int64_t diff_us = target - now;
    return diff_us > 0 ? (int)(diff_us / 1000000LL) : 0;
}

int timer_get_cycle_interval_minutes(void) { return s_cycle_interval_minutes; }
int timer_get_watering_duration_minutes(void) { return s_watering_duration_minutes; }
void timer_register_callback(void (*callback)(void)) { s_cycle_callback = callback; }

static TaskHandle_t s_logging_task = NULL;
static void (*s_logging_callback)(void) = NULL;

static void logging_timer_task(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(15 * 60 * 1000); 
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (s_logging_callback) s_logging_callback();
    }
}

void timer_start_logging(void (*callback)(void)) {
    s_logging_callback = callback;
    if (s_logging_task == NULL) {
        xTaskCreate(logging_timer_task, "log_task", 3072, NULL, 4, &s_logging_task);
    }
}