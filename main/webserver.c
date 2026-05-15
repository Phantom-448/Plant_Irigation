#include "state.h"
#include "webserver.h"
#include "actor.h"
#include <esp_log.h>
#include <esp_http_server.h>

// #include <cJSON.h>
// Ganz oben in webserver.c hinzufügen:
static esp_err_t watering_start_handler(httpd_req_t *req);

static const char *TAG = "WEB_SERVER";

/* Die index.html wird als Binärdaten eingebettet (siehe CMakeLists.txt) */
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

// 1. Handler für die Webseite (Root)
static esp_err_t root_get_handler(httpd_req_t *req) {
    size_t html_len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, html_len);
    return ESP_OK;
}

// 2. API Handler: Status senden (JSON)
static esp_err_t status_get_handler(httpd_req_t *req) {
    // cJSON *root = cJSON_CreateObject();
    
    // // Daten sicher aus dem globalen State holen
    // if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //     cJSON_AddNumberToObject(root, "temperature", sys_state.current_temp);
    //     cJSON_AddNumberToObject(root, "air_humidity", sys_state.air_humidity);
    //     cJSON_AddNumberToObject(root, "soil_moisture_1", 62);
    //     cJSON_AddBoolToObject(root, "pump_running", sys_state.valve_1_state);
    //     cJSON_AddNumberToObject(root, "minutes_to_next_water", 115);
    //     xSemaphoreGive(state_mutex);
    // }

    // const char *sys_info = cJSON_PrintUnformatted(root);
    const char *sys_info = "{\"temperature\": 25.0, \"air_humidity\": 60.0}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, sys_info);
    
    // free((void*)sys_info);
    // cJSON_Delete(root);
    return ESP_OK;
}

// 3. API Handler: Einstellungen empfangen (JSON)
static esp_err_t config_post_handler(httpd_req_t *req) {
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) return ESP_FAIL;
    content[ret] = '\0';

    // cJSON *root = cJSON_Parse(content);
    // if (root) {
    //     if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //         sys_state.watering_duration = cJSON_GetObjectItem(root, "watering_duration_min")->valueint;
    //         // Logik zum Speichern im NVS (Flash) hier aufrufen
    //         xSemaphoreGive(state_mutex);
    //     }
    //     cJSON_Delete(root);
    // }
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    return ESP_OK;
}

static esp_err_t watering_start_handler(httpd_req_t *req) {
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    // cJSON *root = cJSON_Parse(buf);
    // if (root) {
    //     int minutes = cJSON_GetObjectItem(root, "duration_min")->valueint;
        
    //     // JETZT: Übergabe an die Logik
    //     actor_start_timed_watering(minutes); 
        
    //     cJSON_Delete(root);
    // }
    // For now, start with 5 minutes
    actor_start_timed_watering(5);
    httpd_resp_sendstr(req, "{\"status\":\"started\"}");
    return ESP_OK;
}

void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true; // Hilft bei vielen Verbindungen

    ESP_LOGI(TAG, "Starte HTTP-Server...");
    if (httpd_start(&server, &config) == ESP_OK) {
        // Routen registrieren
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t status = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(server, &status);

        httpd_uri_t config_uri = { .uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler };
        httpd_register_uri_handler(server, &config_uri);

        // webserver.c – Zeile anpassen:

        httpd_uri_t watering_uri = { .uri = "/api/start_watering", .method = HTTP_POST, .handler = watering_start_handler };
        httpd_register_uri_handler(server, &watering_uri);

        ESP_LOGI(TAG, "HTTP-Server gestartet und Endpunkte registriert.");
    } else {
        ESP_LOGE(TAG, "HTTP-Server konnte nicht gestartet werden.");
    }
}
