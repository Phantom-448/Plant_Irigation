#include <stdio.h>
#include <time.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "button.h"
#include "secrets.h"
#include "actor.h"
#include "timer.h"
#include "sd_storage.h"
#include "profile_manager.h"
#include "logger.h"
#include "soil.h"
#include "dht22.h"
#include "pin_config.h"



static const char *TAG = "MAIN_APP";

static void timer_cycle_callback(void) {
    ESP_LOGI("SMART_LOGIC", "Starte Profil-Auswertung...");

    int duration = 0;
    int moisture = 0;
    float temp = 0.0f;
    WateringProfile_t current_profile;

    // 1. Snapshot der Daten sicher aus dem State holen
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        moisture = sys_state.soil_moisture_1;
        temp = sys_state.current_temp;
        current_profile = sys_state.active_profile; // Kopie des structs
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE("SMART_LOGIC", "Konnte State nicht lesen!");
        return; 
    }

    // 2. Regel 1: Ist die Erde feucht genug?
    if (moisture > current_profile.min_soil_moisture_percent) {
        ESP_LOGI("SMART_LOGIC", "Erde ist feucht genug (%d%% > %d%%). Wässern übersprungen.", 
                 moisture, current_profile.min_soil_moisture_percent);
        return;
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

static void button_pressed_handler(void *arg)
{
    int watering_duration = timer_get_watering_duration_minutes();
    int cycle_interval = timer_get_cycle_interval_minutes();

    if (watering_duration <= 0) {
        watering_duration = 5;
    }
    if (cycle_interval <= 0) {
        cycle_interval = 60;
    }

    ESP_LOGI(TAG, "Button gedrückt: manuelle Bewässerung starten und Zyklus zurücksetzen");
    actor_start_timed_watering(watering_duration);
    timer_start_cycle(cycle_interval, watering_duration);
}

void sensor_reading_task(void *pvParameters) {
    // Lokale Variablen für die DHT22-Messung mit sicheren Startwerten (Fallbacks)
    float last_valid_temp = 22.0f;
    float last_valid_humidity = 50.0f;

    while (1) {
        float temp = 0.0f;
        float humidity = 0.0f;
        
        // 1. Echte Hardware-Messung des DHT22 starten
        // Wir übergeben den Pin aus pin_config.h und die Adressen der Variablen via Zeiger
        bool dht_ok = dht22_read(GPIO_DHT22, &temp, &humidity);
        
        if (dht_ok) {
            // Wenn die Checksumme stimmt, sichern wir die Werte als Backup
            last_valid_temp = temp;
            last_valid_humidity = humidity;
        } else {
            // Fehlerfall: Keine Antwort vom Sensor oder kaputte Checksumme
            ESP_LOGW("SENSOR_TASK", "Konnte DHT22-Sensor nicht lesen (GPIO %d). Nutze letzten bekannten Wert.", GPIO_DHT22);
            temp = last_valid_temp;
            humidity = last_valid_humidity;
        }

        // 2. Echte Hardware-Messung der Bodenfeuchtigkeit aus soil.c laden
        int soil_moisture = read_soil_moisture();

        ESP_LOGI("SENSOR_TASK", "Sensorwerte - Temp: %.1f°C, Luftfeuchtigkeit: %.1f%%, Bodenfeuchte: %d%%", 
                 temp, humidity, soil_moisture);

        // 3. Werte sicher unter Mutex-Schutz in den globalen Zustand (sys_state) schreiben
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
            sys_state.current_temp = temp;
            sys_state.air_humidity = humidity;
            sys_state.soil_moisture_1 = soil_moisture;
            xSemaphoreGive(state_mutex);
        } else {
            ESP_LOGE("SENSOR_TASK", "Konnte Mutex nicht sperren. State nicht aktualisiert!");
        }

        // Alle 60 Sekunden erneut messen
        vTaskDelay(pdMS_TO_TICKS(60000)); 
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starte Garten-Bewässerung auf ESP32-C3...");

    // 1. Basis-Subsysteme initialisieren
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    state_init();
    actor_init();        

    // 2. Peripherie & Taster
    if (!button_init(GPIO_BUTTON)) {
        ESP_LOGW(TAG, "Taster konnte nicht initialisiert werden");
    } else {
        button_set_callback(button_pressed_handler, NULL);
    }

    // 3. Speicher (SD-Karte)
    init_sd_card();   
    xTaskCreate(sd_card_monitor_task, "sd_monitor", 3072, NULL, 5, NULL);

    if (sd_card_self_test()) {
        ESP_LOGI(TAG, "SD-Karten-Selbsttest erfolgreich.");
        scan_profiles_on_sd();
    } else {
        ESP_LOGW(TAG, "SD-Karten-Selbsttest fehlgeschlagen.");
    }

    // 4. Zeitsteuerung & Logik
    timer_init();
    timer_register_callback(timer_cycle_callback);
    
    int cycle_interval_minutes = 60;
    int watering_duration_minutes = 5;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        cycle_interval_minutes = sys_state.active_profile.check_interval_minutes;
        watering_duration_minutes = sys_state.active_profile.base_watering_minutes;
        xSemaphoreGive(state_mutex);
    }

    timer_start_cycle(cycle_interval_minutes, watering_duration_minutes);
    timer_start_logging(logger_write_sensor_data);

    // 5. Netzwerk-Infrastruktur
    if (wifi_init_sta()) {
        start_mdns_service();
        start_webserver();
    }

    // 6. Hintergrund-Tasks starten
    xTaskCreate(sensor_reading_task, "sensor_task", 3072, NULL, 5, NULL);

    // 7. Status-LED Konfiguration
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GPIO_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    // Unendliche Heartbeat-Schleife (app_main Task bleibt bestehen)
    bool led_state = false;
    while (1) {
        led_state = !led_state;
        gpio_set_level(GPIO_LED, led_state ? 1 : 0);
        ESP_LOGD(TAG, "Heartbeat - LED %s", led_state ? "ON" : "OFF"); // Auf DEBUG geändert, um Log nicht zu fluten
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}