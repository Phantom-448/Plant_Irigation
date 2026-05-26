// In actor.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pin_config.h"
#include <esp_log.h>
#include <stdbool.h>

static bool s_actor_state = false;


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