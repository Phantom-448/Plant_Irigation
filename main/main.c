#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <stdio.h>

// Deine Module
#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "temp.h"
#include "humid.h"
#include "actor.h"
#include "timer.h"
#include "sd_storage.h"

static const char *TAG = "MAIN_APP";



static void timer_cycle_callback(void) {
    int duration = timer_get_watering_duration_minutes();
    ESP_LOGI(TAG, "Timer löst Bewässerung aus: Dauer %d Minuten", duration);
    actor_start_timed_watering(duration);
}

/**
 * Sensor-Task: Liest periodisch die Daten aus.
 * FreeRTOS ermöglicht die Ausführung mehrerer Tasks gleichzeitig.
 */
void sensor_reading_task(void *pvParameters) {
    while (1) {
        // Temperatur vom internen System-Sensor lesen und filtern
        // ESP32C3 hat keinen internen Temp-Sensor, daher dummy Wert
        float t = 25.0f; // temp_sensor_read_filtered();
        
        // Luftfeuchtigkeit lesen und filtern
        float h = humid_sensor_read_filtered();

        ESP_LOGI(TAG, "Sensoren aktualisiert: Temp=%.2f°C, Humid=%.2f%%", t, h);

        // Alle 5 Sekunden messen
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void mqtt_app_start(void) {
    ESP_LOGI(TAG, "MQTT stub: mqtt_app_start() called, no MQTT implementation available.");
}

void log_sensor_data_callback(void) {
    // Zeitstempel generieren
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char log_buffer[128];
    
    // Daten sicher aus dem globalen State holen
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(log_buffer, sizeof(log_buffer), "%s,%.1f,%.1f,%d,%d",
                 time_str,
                 sys_state.current_temp,
                 sys_state.air_humidity,
                 sys_state.soil_moisture_1,
                 sys_state.valve_1_state ? 1 : 0);
                 
        xSemaphoreGive(state_mutex);
        
        // Auf die SD-Karte schreiben
        sd_write_log(log_buffer);
        ESP_LOGI("MAIN", "Automatischer Log geschrieben: %s", log_buffer);
    }
}


// Funktion zum Schreiben des Logs, die von main.c aufgerufen wird
void sd_write_log(const char* log_line) {
    // "a" steht für Append. Neue Daten werden unten angehängt.
    // Wenn die Datei nicht existiert, wird sie automatisch erstellt.
    FILE* f = fopen("/sdcard/logs/sensor_log.csv", "a");
    
    if (f == NULL) {
        ESP_LOGE(TAG, "Fehler: Konnte Log-Datei nicht öffnen!");
        return;
    }
    
    // Daten in die Datei schreiben und Zeilenumbruch anfügen
    fprintf(f, "%s\n", log_line);
    
    // WICHTIG: Sofort schließen, um Datenverlust bei Stromausfall zu verhindern
    fclose(f); 
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
    ESP_LOGI(TAG, "NVS erfolgreich initialisiert.");

    // 2. Globalen Systemstatus & Mutex initialisieren[cite: 1]
    state_init();
    ESP_LOGI(TAG, "Systemzustand und Mutex initialisiert.");

    // 3. Hardware-Peripherie initialisieren (I2C-Bus & Sensoren)[cite: 1]
    // temp_sensor_init();  // Interne System-Sensorik - not supported on ESP32C3
    ESP_LOGI(TAG, "Initialisiere Feuchtigkeitssensor...");
    humid_sensor_init(); // Externer Feuchtigkeitssensor
    ESP_LOGI(TAG, "Feuchtigkeitssensor initialisiert.");
    ESP_LOGI(TAG, "Initialisiere Aktor...");
    actor_init();        // Aktor-Initialisierung
    ESP_LOGI(TAG, "Aktor initialisiert.");

    // 3b. Timer für zyklische Bewässerung initialisieren
    timer_init();
    timer_register_callback(timer_cycle_callback);
    timer_start_cycle(60, 5); // Zyklus: alle 60 Minuten, Bewässerungsdauer 5 Minuten
    ESP_LOGI(TAG, "Zyklischer Bewässerungstimer gestartet.");

    // 4. Netzwerk-Verbindungen herstellen
    ESP_LOGI(TAG, "Starte WLAN-Station...");
    wifi_init_sta();     // WLAN starten
    mqtt_app_start();    // Home Assistant MQTT-Anbindung starten

    // 5. Webserver wird automatisch in wifi_init_sta() gestartet
    // nach erfolgreicher WiFi-Verbindung

    // 6. Sensor-Lese-Task starten
    // Wir nutzen FreeRTOS, um die Sensoren im Hintergrund zu verarbeiten[cite: 1].
    xTaskCreate(sensor_reading_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System erfolgreich gestartet. Web-Dashboard bereit.");
}
