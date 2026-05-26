#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BUTTON";
static button_pressed_callback_t s_button_callback = NULL;
static void *s_button_callback_arg = NULL;
static gpio_num_t s_button_pin = 0;
static bool s_button_pin_configured = false;

static void button_task(void *pvParameters)
{
    bool last_state = false;
    while (1) {
        if (!s_button_pin_configured) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        bool current_state = gpio_get_level(s_button_pin) == 1;
        if (current_state && !last_state) {
            ESP_LOGI(TAG, "Button pressed on GPIO %d", s_button_pin);
            if (s_button_callback) {
                s_button_callback(s_button_callback_arg);
            }
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool button_init(gpio_num_t pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    s_button_pin = pin;
    s_button_pin_configured = true;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if (xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Button task could not be created");
        return false;
    }

    ESP_LOGI(TAG, "Button hardware initialized on GPIO %d", pin);
    return true;
}

bool button_set_callback(button_pressed_callback_t callback, void *arg)
{
    s_button_callback = callback;
    s_button_callback_arg = arg;
    return true;
}

bool button_is_pressed(gpio_num_t pin)
{
    return gpio_get_level(pin) == 1;
}
