#include "soil.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "soil";
static bool s_initialized = false;

#define SOIL_RAW_MIN_CONNECTED 5
#define SOIL_RAW_MAX_CONNECTED 4090

static adc_oneshot_unit_handle_t adc1_handle;

static void soil_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SOIL_ADC_CHANNEL, &config));

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, SOIL_ADC_CHANNEL, &raw);
    
    s_initialized = true;
}

int read_soil_moisture(void)
{
    if (!s_initialized) {
        soil_init();
    }

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, SOIL_ADC_CHANNEL, &raw);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fehler beim Lesen des ADC");
        return -1;
    }

    /* ESP32-C3 hat einen 12-bit ADC -> Max Wert ist 4095 */
    float pct = ((float)raw / 4095.0f) * 100.0f;
    
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    #if SOIL_SENSOR_INVERT
        pct = 100.0f - pct;
    #endif

    return (int)pct;
}