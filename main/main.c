#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <stdio.h>
#include "cJSON.h"

#include "state.h"
#include "wlan.h"
#include "webserver.h"
#include "button.h"
#include "secrets.h"
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

xTaskHandle_t sensor_reading_task_handle = NULL;
void sensor_reading_task(void *pvParameters) {
    while (1) {
        float temp = read_temperature();
        float humidity = read_humidity();
        int soil_moisture = read_soil_moisture();
        ESP_LOGI("SENSOR_TASK", "Sensorwerte - Temp: %.1f°C, Luftfeuchtigkeit: %.1f%%, Bodenfeuchte: %d%%", 
                 temp, humidity, soil_moisture);

        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sys_state.current_temp = temp;
            sys_state.current_humidity = humidity;
            sys_state.soil_moisture_1 = soil_moisture;
            xSemaphoreGive(state_mutex);
        } else {
            ESP_LOGE("SENSOR_TASK", "Konnte State nicht aktualisieren!");
        }
        vTaskDelay(pdMS_TO_TICKS(60000)); // Alle 60 Sekunden lesen
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starte Garten-Bewässerung auf ESP32-C3...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS erfolgreich initialisiert.");

    state_init();
    ESP_LOGI(TAG, "Systemzustand und Mutex initialisiert.");

    temp_sensor_init();
    ESP_LOGI(TAG, "Temperatursensor initialisiert.");
    ESP_LOGI(TAG, "Initialisiere Feuchtigkeitssensor...");
    humid_sensor_init(); 
    ESP_LOGI(TAG, "Feuchtigkeitssensor initialisiert.");
    ESP_LOGI(TAG, "Initialisiere Aktor...");
    actor_init();        
    ESP_LOGI(TAG, "Aktor initialisiert.");

    if (!button_init(GPIO_BUTTON)) {
        ESP_LOGW(TAG, "Taster konnte nicht initialisiert werden");
    } else {
        button_set_callback(button_pressed_handler, NULL);
    }

    init_sd_card();   
    xTaskCreate(sd_card_monitor_task, "sd_monitor", 4096, NULL, 5, NULL);

    if (sd_card_self_test()) {
        ESP_LOGI(TAG, "SD-Karten-Selbsttest erfolgreich.");
    } else {
        ESP_LOGW(TAG, "SD-Karten-Selbsttest fehlgeschlagen oder SD nicht verfügbar.");
    }
    scan_profiles_on_sd();

    timer_init();
    timer_register_callback(timer_cycle_callback);
    
    int cycle_interval_minutes = 60;
    int watering_duration_minutes = 5;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        cycle_interval_minutes = sys_state.active_profile.check_interval_minutes;
        watering_duration_minutes = sys_state.active_profile.base_watering_minutes;
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGW(TAG, "Konnte aktives Profil nicht lesen, verwende Standardwerte");
    }

    timer_start_cycle(cycle_interval_minutes, watering_duration_minutes);
    timer_start_logging(logger_write_sensor_data);
    ESP_LOGI(TAG, "Zyklischer Bewässerungstimer gestartet.");

    ESP_LOGI(TAG, "Starte WLAN-Station...");
    if (wifi_init_sta()) {
        ESP_LOGI(TAG, "Verbunden mit SSID: %s", WIFI_SSID);
        start_mdns_service();
        start_webserver();
    } else {
        ESP_LOGI(TAG, "Verbindung zu SSID: %s fehlgeschlagen", WIFI_SSID);
    }

    xTaskCreate(sensor_reading_task, "sensor_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "System erfolgreich gestartet. Web-Dashboard bereit.");
}
