#include "soil.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "soil";
static bool s_initialized = false;
static bool s_simulate = false;

/* default thresholds to detect an unconnected sensor */
#define SOIL_RAW_MIN_CONNECTED 5
#define SOIL_RAW_MAX_CONNECTED 4090

// Neuer ADC Handle für ESP-IDF v5/v6
static adc_oneshot_unit_handle_t adc1_handle;

static void soil_init(void)
{
    // 1. ADC Einheit 1 initialisieren
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2. ADC Kanal konfigurieren (Mit 12dB Attenuation für volle 0-3.3V Reichweite)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SOIL_ADC_CHANNEL, &config));

    // 3. Quick probe (Testmessung)
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, SOIL_ADC_CHANNEL, &raw);
    
    if (ret != ESP_OK || raw < 0) {
        ESP_LOGW(TAG, "ADC probe failed, enabling simulation");
        s_simulate = true;
    } else if (raw <= SOIL_RAW_MIN_CONNECTED || raw >= SOIL_RAW_MAX_CONNECTED) {
        ESP_LOGW(TAG, "ADC probe value %d looks like disconnected sensor, enabling simulation", raw);
        s_simulate = true;
    } else {
        s_simulate = false;
        ESP_LOGI(TAG, "Soil sensor detected (raw=%d)", raw);
    }

    s_initialized = true;
}

static int soil_simulated_percent(void)
{
    /* produce a smooth, slow oscillation between roughly 30% and 90% */
    int64_t us = esp_timer_get_time();
    float t = (float)us / 1e6f; /* seconds */
    const float period = 60.0f; /* 60s period */
    float angle = (2.0f * (float)M_PI / period) * t;
    float v = 0.5f + 0.5f * sinf(angle); /* 0..1 */
    float pct = 30.0f + v * 60.0f; /* 30..90 */
    return (int)(pct + 0.5f);
}

int read_soil_moisture(void)
{
    if (!s_initialized) {
        soil_init();
    }

    if (s_simulate) {
        return soil_simulated_percent();
    }

    // Echte Messung über die neue API
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, SOIL_ADC_CHANNEL, &raw);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fehler beim Lesen des ADC");
        return -1;
    }

    /* raw in Prozent (0..100) umrechnen */
    /* ESP32-C3 hat einen 12-bit ADC -> Max Wert ist 4095 */
    float pct = ((float)raw / 4095.0f) * 100.0f;
    
    // Begrenzung, damit die Werte nicht über 100 oder unter 0 springen
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    #if SOIL_SENSOR_INVERT
        pct = 100.0f - pct;
    #endif

    return (int)pct;
}