
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <esp_log.h>

/**
 * Initialisiert den Feuchtigkeitssensor (I2C-Bus Setup)
 */
void humid_sensor_init(void);

/**
 * Liest die aktuelle Luftfeuchtigkeit und wendet einen Filter an
 * @return Luftfeuchtigkeit in Prozent (0.0 - 100.0)
 */
float humid_sensor_read_filtered(void);