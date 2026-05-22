#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <stdio.h>
#include "cjson.h"

#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "temp.h"
#include "humid.h"
#include "actor.h"
#include "timer.h"
#include "sd_storage.h"
#include "profile_manager.h"
#include "logger.h"


static const char *TAG = "MAIN_APP";

static void timer_cycle_callback(void) {
    ESP_LOGI("SMART_LOGIC", "Starte Profil-Auswertung...");

    int duration = 0;
    int moisture = 0;
    float temp = 0.0f;
    WateringProfile_t current_profile;

    // 1. Snapshot der Daten sicher aus dem State holen
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        moisture = sys_state.soil_moisture_1;
        temp = sys_state.current_temp;
        current_profile = sys_state.active_profile; // Kopie des structs
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE("SMART_LOGIC", "Konnte State nicht lesen!");
        return; 
    }

    // 2. Regel 1: Ist die Erde feucht genug?
    // Wenn Feuchtigkeit HÖHER als der Schwellenwert ist -> Abbruch, nicht wässern!
    if (moisture > current_profile.min_soil_moisture_percent) {
        ESP_LOGI("SMART_LOGIC", "Erde ist feucht genug (%d%% > %d%%). Wässern übersprungen.", 
                 moisture, current_profile.min_soil_moisture_percent);
        return; // Callback beenden!
    }

    // 3. Regel 2: Dauer berechnen
    duration = current_profile.base_watering_minutes;
    ESP_LOGI("SMART_LOGIC", "Erde zu trocken. Basis-Dauer: %d Min.", duration);

    // 4. Regel 3: Wetter/Temperatur-Anpassung
    if (temp >= current_profile.hot_temp_threshold) {
        duration += current_profile.hot_temp_extra_minutes;
        ESP_LOGI("SMART_LOGIC", "Es ist heiß (%.1f°C >= %.1f°C)! Addiere %d Extra-Minuten. Neue Dauer: %d Min.", 
                 temp, current_profile.hot_temp_threshold, current_profile.hot_temp_extra_minutes, duration);
    }

    // 5. Befehl an den Aktor senden
    ESP_LOGI("SMART_LOGIC", "Auswertung beendet. Starte Pumpe für %d Minuten.", duration);
    actor_start_timed_watering(duration);
}

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
    ESP_LOGI(TAG, "NVS erfolgreich initialisiert.");

    // 2. Globalen Systemstatus & Mutex initialisieren[cite: 1]
    state_init();
    ESP_LOGI(TAG, "Systemzustand und Mutex initialisiert.");

    init_sd_card();      // SD-Karte initialisieren
    scan_profiles_on_sd(); // SD-Karte nach Bewässerungsprofilen durchsuchen

    // 3. Hardware-Peripherie initialisieren (I2C-Bus & Sensoren)
    temp_sensor_init();
    ESP_LOGI(TAG, "Temperatursensor initialisiert.");
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
    timer_start_logging(logger_write_sensor_data);
    ESP_LOGI(TAG, "Zyklischer Bewässerungstimer gestartet.");

    // 4. Netzwerk-Verbindungen herstellen
    ESP_LOGI(TAG, "Starte WLAN-Station...");
    wifi_init_sta();     // WLAN starten


    // 5. Webserver wird automatisch in wifi_init_sta() gestartet
    // nach erfolgreicher WiFi-Verbindung

    // 6. Sensor-Lese-Task starten
    // Wir nutzen FreeRTOS, um die Sensoren im Hintergrund zu verarbeiten[cite: 1].
    xTaskCreate(sensor_reading_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System erfolgreich gestartet. Web-Dashboard bereit.");
}
