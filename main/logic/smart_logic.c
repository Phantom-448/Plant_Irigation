#include "smart_logic.h"
#include "state.h"
#include "timer.h"
#include "actor.h"
#include "pin_config.h"
#include "esp_log.h"

static const char *TAG = "SMART_LOGIC";

void smart_logic_evaluate(void) {
    ESP_LOGI(TAG, "Starte dynamische Profil-Auswertung...");

    int duration = 0;
    int moisture = 0;
    float temp = 0.0f;
    float humidity = 0.0f;
    WateringProfile_t current_profile;

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        moisture = sys_state.soil_moisture_1;
        temp = sys_state.current_temp;
        humidity = sys_state.air_humidity;
        current_profile = sys_state.active_profile;
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE(TAG, "Konnte State nicht lesen!");
        return; 
    }

    if (moisture < 0 || moisture > 100) {
        ESP_LOGE(TAG, "Sensor-Fehler (%d%%). Bewässerung blockiert.", moisture);
        return;
    }

    if (moisture > (current_profile.min_soil_moisture_percent - 5)) {
        ESP_LOGI(TAG, "Feuchtigkeit ausreichend (%d%%). Ziel-Minimum: %d%%.", 
                 moisture, current_profile.min_soil_moisture_percent);
        
        int next_check = current_profile.check_interval_minutes;
        if (temp > current_profile.hot_temp_threshold && humidity < 40.0f) {
            next_check /= 2; 
            ESP_LOGI(TAG, "Heiß und trocken: Nächster Check bereits in %d Min.", next_check);
        }
        

        timer_start_cycle(next_check, 0); 
        return;
    }

    float evap_factor = 1.0f;
    
    if (temp > current_profile.hot_temp_threshold) {
        evap_factor += (temp - current_profile.hot_temp_threshold) * 0.1f; 
    }
    if (humidity < 40.0f) {
        evap_factor += 0.2f; 
    } else if (humidity > 80.0f) {
        evap_factor -= 0.2f; 
    }

    if (evap_factor > 2.0f) evap_factor = 2.0f;
    if (evap_factor < 0.5f) evap_factor = 0.5f;

    int target_moisture = current_profile.min_soil_moisture_percent + 20;
    if (target_moisture > 100) target_moisture = 100;
    
    int deficit = target_moisture - moisture;
    
    float calculated_duration = (float)current_profile.base_watering_minutes * ((float)deficit / 20.0f) * evap_factor;
    duration = (int)(calculated_duration + 0.5f); 

    int max_duration = current_profile.base_watering_minutes * 3;
    if (duration > max_duration) duration = max_duration;
    if (duration < 1) duration = 1;

    ESP_LOGI(TAG, "Auswertung: Defizit=%d%%, Evap-Faktor=%.2f -> Berechnete Dauer: %d Min.", 
             deficit, evap_factor, duration);

    actor_start_timed_watering(duration);
    timer_start_cycle(current_profile.check_interval_minutes, duration);
}