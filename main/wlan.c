#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "webserver.h"
#include "secrets.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "esp_log.h"
#include "esp_phy.h"

static const char *TAG = "WLAN_SERVICE";

void start_mdns_service() {
    // 1. mDNS initialisieren
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "mDNS Init fehlgeschlagen: %d", err);
        return;
    }

    // 2. Hostnamen festlegen (ergibt http://bewaesserung.local)
    ESP_ERROR_CHECK(mdns_hostname_set("bewaesserung"));
    
    // 3. Instanz-Namen festlegen (für die Anzeige in Netzwerk-Tools)
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32-C3 Balkon-Bewässerung"));

    // 4. Den HTTP-Dienst bekannt geben
    // Damit wissen andere Geräte im Netz: "Hier läuft ein Webserver auf Port 80"
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS gestartet: http://bewaesserung.local");
}


// Wir nutzen ein Event-Group, um zu signalisieren, wenn wir verbunden sind
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGI(TAG, "Disconnected from AP, reason: %d", event->reason);
        if (s_retry_num < 5) { // Versuche es 5 Mal
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Versuche erneut mit dem AP zu verbinden...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP erhalten: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Initialisiere Netzwerk-Subsystem...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            // Diese Werte werden aus secrets.h geladen
            .ssid = MY_WIFI_SSID,
            .password = MY_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_LOGI(TAG, "Verbinde zu SSID '%s'...", MY_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "WiFi-Station gestartet, Verbindungsversuch läuft.");

    // Warten, bis die Verbindung steht oder fehlgeschlagen ist
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Verbunden mit SSID: %s", MY_WIFI_SSID);
        // Innerhalb der WLAN-Event-Logik oder nach wifi_init_sta()
        start_mdns_service(); // Hier aktivieren
        start_webserver();

    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Verbindung zu SSID: %s fehlgeschlagen", MY_WIFI_SSID);
    }
}
