#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "pin_config.h"

static const char *TAG = "BUTTON";
static button_pressed_callback_t s_button_callback = NULL;
static void *s_button_callback_arg = NULL;
static gpio_num_t s_button_pin = GPIO_BUTTON;
static bool s_button_pin_configured = false;
static QueueHandle_t s_button_queue = NULL;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_button_queue) {
        xQueueSendFromISR(s_button_queue, &gpio_num, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *pvParameters)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(s_button_queue, &io_num, portMAX_DELAY) == pdTRUE) {
            
            if (gpio_get_level((gpio_num_t)io_num) == 1) {
                
                ESP_LOGI(TAG, "Button press detected on GPIO %d", io_num);
                if (s_button_callback) {
                    s_button_callback(s_button_callback_arg);
                }
                
                vTaskDelay(pdMS_TO_TICKS(150));
                
                xQueueReset(s_button_queue);
            }
        }
    }
}

bool button_init(gpio_num_t pin)
{
    if (s_button_pin_configured) {
        ESP_LOGW(TAG, "button_init: button already configured on GPIO %d", s_button_pin);
        return true;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return false;
    }

    s_button_queue = xQueueCreate(1, sizeof(uint32_t));
    if (s_button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return false;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        return false;
    }
    
    gpio_isr_handler_add(pin, button_isr_handler, (void *)(uintptr_t)pin);

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    s_button_pin = pin;
    s_button_pin_configured = true;

    ESP_LOGI(TAG, "Button initialized on GPIO %d", pin);
    return true;
}

void button_set_callback(button_pressed_callback_t cb, void *arg)
{
    s_button_callback = cb;
    s_button_callback_arg = arg;
}