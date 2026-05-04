#include <stdio.h>
#include "nvs_flash.h"
#include "esp_wifi.h" // (und weitere WLAN Header)

// Unsere eigenen Module
#include "state.h"
#include "mcp23017.h"
#include "webserver.h"
#include "mqtt_ha.h"

void app_main(void) {
    // 1. NVS (Flash-Speicher) starten - Pflicht für WLAN!
    nvs_flash_init();

    // 2. Gemeinsamen Status & Mutex initialisieren
    state_init();

    // 3. Hardware starten
    mcp_init();

    // 4. WLAN verbinden (hier würdest du deine WLAN-Funktion aufrufen)
    // wifi_init_sta(); 

    // 5. Dienste starten
    webserver_start();
    mqtt_app_start();
}