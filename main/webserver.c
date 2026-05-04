#include "webserver.h"
#include "state.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "WEB_SERVER";

// Diese Zeilen machen die eingebettete HTML-Datei im C-Code verfügbar
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

// 1. Handler: Liefert die index.html aus
static esp_err_t get_root_handler(httpd_req_t *req) {
    size_t html_len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, html_len);
}

// 2. Handler: Schickt Sensor-Daten als JSON zum Browser (GET /api/status)
static esp_err_t get_status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    // Wir holen uns die Daten sicher über den Mutex
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        cJSON_AddNumberToObject(root, "temperature", 24.5); // Hier später echte Sensorwerte rein
        cJSON_AddNumberToObject(root, "air_humidity", 45);
        cJSON_AddNumberToObject(root, "soil_moisture_1", 60);
        cJSON_AddBoolToObject(root, "pump_running", sys_state.valve_1_state);
        cJSON_AddNumberToObject(root, "minutes_to_next_water", 120);
        cJSON_AddNumberToObject(root, "timer_max_minutes", 240);
        xSemaphoreGive(state_mutex);
    }

    const char *sys_info = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, sys_info);
    
    free((void*)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

// 3. Handler: Empfängt Einstellungen vom Browser (POST /api/config)
static esp_err_t post_config_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sys_state.watering_duration = cJSON_GetObjectItem(root, "watering_duration_min")->valueint;
            // Hier auch PWM-Wert für Pumpe speichern
            ESP_LOGI(TAG, "Neue Dauer gespeichert: %d min", sys_state.watering_duration);
            xSemaphoreGive(state_mutex);
        }
        cJSON_Delete(root);
    }
    
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// Server initialisieren und URI-Pfade registrieren
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Pfad: /
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = get_root_handler };
        httpd_register_uri_handler(server, &root_uri);

        // Pfad: /api/status
        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = get_status_handler };
        httpd_register_uri_handler(server, &status_uri);

        // Pfad: /api/config
        httpd_uri_t config_uri = { .uri = "/api/config", .method = HTTP_POST, .handler = post_config_handler };
        httpd_register_uri_handler(server, &config_uri);
    }
    return server;
}