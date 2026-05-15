#include <stdbool.h>
#include "state.h"
#include "webserver.h"
#include "actor.h"
#include "timer.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <string.h>

// #include <cJSON.h>
// Ganz oben in webserver.c hinzufügen:
static esp_err_t watering_start_handler(httpd_req_t *req);
static esp_err_t relay_post_handler(httpd_req_t *req);
static esp_err_t cycle_post_handler(httpd_req_t *req);

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
static bool parse_json_int(const char *src, const char *key, int *out) {
    const char *p = strstr(src, key);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '"') p++;
    *out = atoi(p);
    return true;
}

static bool parse_json_bool(const char *src, const char *key, bool *out) {
    const char *p = strstr(src, key);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '"') p++;
    if (strncmp(p, "true", 4) == 0) {
        *out = true;
    } else if (strncmp(p, "false", 5) == 0) {
        *out = false;
    } else {
        return false;
    }
    return true;
}

static esp_err_t status_get_handler(httpd_req_t *req) {
    int seconds_to_next = timer_get_seconds_to_next_cycle();
    bool pump_running = actor_get_state();
    int cycle_interval = timer_get_cycle_interval_minutes();
    int watering_duration = timer_get_watering_duration_minutes();

    char response[256];
    int len = snprintf(response, sizeof(response),
        "{\"temperature\": 25.0, \"air_humidity\": 60.0, \"soil_moisture_1\": 62, \"pump_running\": %s, \"seconds_to_next_water\": %d, \"cycle_interval_minutes\": %d, \"watering_duration_minutes\": %d}",
        pump_running ? "true" : "false",
        seconds_to_next,
        cycle_interval,
        watering_duration);

    if (len < 0 || len >= sizeof(response)) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, len);
    return ESP_OK;
}

// 3. API Handler: Einstellungen empfangen (JSON)
static esp_err_t cycle_post_handler(httpd_req_t *req) {
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) return ESP_FAIL;
    content[ret] = '\0';

    int interval = 0;
    int duration = 0;
    bool ok = false;

    if (parse_json_int(content, "cycle_interval_minutes", &interval) &&
        parse_json_int(content, "watering_duration_minutes", &duration)) {
        ok = timer_start_cycle(interval, duration);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, ok ? "{\"status\":\"cycle_set\"}" : "{\"status\":\"failed\"}");
    return ESP_OK;
}

static esp_err_t relay_post_handler(httpd_req_t *req) {
    char content[64];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) return ESP_FAIL;
    content[ret] = '\0';

    bool state = false;
    if (parse_json_bool(content, "state", &state)) {
        actor_set_relay(state);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, state ? "{\"status\":\"relay_on\"}" : "{\"status\":\"relay_off\"}");
    return ESP_OK;
}

static esp_err_t watering_start_handler(httpd_req_t *req) {
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    int minutes = 5;
    parse_json_int(buf, "duration_min", &minutes);

    actor_start_timed_watering(minutes);
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

        httpd_uri_t cycle_uri = { .uri = "/api/cycle", .method = HTTP_POST, .handler = cycle_post_handler };
        httpd_register_uri_handler(server, &cycle_uri);

        httpd_uri_t relay_uri = { .uri = "/api/relay", .method = HTTP_POST, .handler = relay_post_handler };
        httpd_register_uri_handler(server, &relay_uri);

        httpd_uri_t watering_uri = { .uri = "/api/start_watering", .method = HTTP_POST, .handler = watering_start_handler };
        httpd_register_uri_handler(server, &watering_uri);

        ESP_LOGI(TAG, "HTTP-Server gestartet und Endpunkte registriert.");
    } else {
        ESP_LOGE(TAG, "HTTP-Server konnte nicht gestartet werden.");
    }
}
