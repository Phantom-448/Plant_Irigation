#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Deine Module
#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "mqtt_ha.h"
#include "mcp23017.h"
#include "temp.h"
#include "humid.h"

static const char *TAG = "MAIN_APP";

/**
 * Sensor-Task: Liest periodisch die Daten aus.
 * FreeRTOS ermöglicht die Ausführung mehrerer Tasks gleichzeitig.
 */
void sensor_reading_task(void *pvParameters) {
    while (1) {
        // Temperatur vom internen System-Sensor lesen und filtern
        float t = temp_sensor_read_filtered();
        
        // Luftfeuchtigkeit lesen und filtern
        float h = humid_sensor_read_filtered();

        ESP_LOGI(TAG, "Sensoren aktualisiert: Temp=%.2f°C, Humid=%.2f%%", t, h);

        // Alle 5 Sekunden messen
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starte Garten-Bewässerung auf ESP32-C3...");

    // 1. NVS (Flash) initialisieren (Pflicht für WLAN-Datenhaltung)[cite: 1]
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Globalen Systemstatus & Mutex initialisieren[cite: 1]
    state_init();

    // 3. Hardware-Peripherie initialisieren (I2C-Bus & Sensoren)[cite: 1]
    mcp_init();          // Port-Expander für die Ventile
    temp_sensor_init();  // Interne System-Sensorik[cite: 1]
    humid_sensor_init(); // Externer Feuchtigkeitssensor

    // 4. Netzwerk-Verbindungen herstellen
    wifi_init_sta();     // WLAN starten
    mqtt_app_start();    // Home Assistant MQTT-Anbindung starten

    // 5. Dienste starten
    start_webserver();   // Dashboard & REST-API bereitstellen

    // 6. Sensor-Lese-Task starten
    // Wir nutzen FreeRTOS, um die Sensoren im Hintergrund zu verarbeiten[cite: 1].
    xTaskCreate(sensor_reading_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System erfolgreich gestartet. Web-Dashboard bereit.");
}