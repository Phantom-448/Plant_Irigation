#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "logic/state.h"
#include "webserver.h"
#include "actor.h"
#include "timer.h"
#include "cJSON.h"
#include "profile_manager.h"
#include "sd_storage.h"
#include "pin_config.h"
#include <esp_log.h>
#include <esp_http_server.h>

static esp_err_t watering_start_handler(httpd_req_t *req);
static esp_err_t relay_post_handler(httpd_req_t *req);
static esp_err_t cycle_post_handler(httpd_req_t *req);
static esp_err_t profiles_list_get_handler(httpd_req_t *req);
static esp_err_t profile_activate_post_handler(httpd_req_t *req);
static esp_err_t favicon_get_handler(httpd_req_t *req);
static esp_err_t sdcard_test_handler(httpd_req_t *req);

static const char *TAG = "WEB_SERVER";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

static esp_err_t root_get_handler(httpd_req_t *req) {
    size_t html_len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, html_len);
    return ESP_OK;
}

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
    float temperature = 0.0f;
    float air_humidity = 0.0f;
    float capacitive_humidity = 0.0f;
    int soil_moisture = 0;
    int seconds_to_next = timer_get_seconds_to_next_cycle();
    bool pump_running = sys_state.valve_1_state;
    int cycle_interval = timer_get_cycle_interval_minutes();
    int watering_duration = timer_get_watering_duration_minutes();

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        temperature = sys_state.current_temp;
        air_humidity = sys_state.air_humidity;
        soil_moisture = sys_state.soil_moisture_1;
        xSemaphoreGive(state_mutex);
    }

    char response[256];
    int len = snprintf(response, sizeof(response),
        "{\"temperature\": %.1f, \"air_humidity\": %.1f, \"capacitive_humidity\": %.1f, \"soil_moisture_1\": %d, \"pump_running\": %s, \"seconds_to_next_water\": %d, \"cycle_interval_minutes\": %d, \"watering_duration_minutes\": %d}",
        temperature,
        air_humidity,
        capacitive_humidity,
        soil_moisture,
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

static esp_err_t cycle_post_handler(httpd_req_t *req) {
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
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
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
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
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    int minutes = 5;
    parse_json_int(buf, "duration_min", &minutes);

    actor_start_timed_watering(minutes);
    httpd_resp_sendstr(req, "{\"status\":\"started\"}");
    return ESP_OK;
}

#include <sys/param.h>

static esp_err_t serve_file_from_sd(httpd_req_t *req, const char *filepath, const char *content_type) {
    ESP_LOGI(TAG, "Serve file from SD: %s", filepath);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "Datei nicht gefunden: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (content_type) {
        httpd_resp_set_type(req, content_type);
    }

    char chunk[1024];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            ESP_LOGE(TAG, "Fehler beim Senden des Datei-Chunks");
            fclose(f);
            return ESP_FAIL;
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);
    return ESP_OK;
}

static esp_err_t api_logs_handler(httpd_req_t *req) {
    const char *filepath = SD_LOG_FILE;
    return serve_file_from_sd(req, filepath, "text/csv");
}

static esp_err_t image_get_handler(httpd_req_t *req) {
    
    char filepath[128];
    const char *filename = req->uri + 5; // Versatz um 5 Zeichen wegen "/img/"
    
    snprintf(filepath, sizeof(filepath), "/sdcard/profiles/%s", filename);
    
    ESP_LOGI(TAG, "Suche Bild auf SD: %s", filepath);
    
    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Bild nicht gefunden: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_OK; 
    }
    fclose(f);

    return serve_file_from_sd(req, filepath, strstr(filepath, ".png") ? "image/png" : "image/jpeg");
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {

    return serve_file_from_sd(req, "/sdcard/favicon.ico", "image/x-icon");
}

static esp_err_t sdcard_test_handler(httpd_req_t *req) {
    bool ok = sd_card_self_test();
    httpd_resp_set_type(req, "application/json");
    if (ok) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"SD-Karten-Selbsttest bestanden\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"SD-Karten-Selbsttest fehlgeschlagen\"}");
    }
    return ESP_OK;
}

void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 16;  
    config.lru_purge_enable = true; 
    config.uri_match_fn = httpd_uri_match_wildcard;
    

    ESP_LOGI(TAG, "Starte HTTP-Server...");
    if (httpd_start(&server, &config) == ESP_OK) {
        
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

        httpd_uri_t profiles_list_uri = { .uri = "/api/profiles", .method = HTTP_GET, .handler = profiles_list_get_handler };
        httpd_register_uri_handler(server, &profiles_list_uri);

        httpd_uri_t profile_activate_uri = { .uri = "/api/profile/activate", .method = HTTP_POST, .handler = profile_activate_post_handler };
        httpd_register_uri_handler(server, &profile_activate_uri);

        httpd_uri_t logs_uri = { .uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_handler };
        httpd_register_uri_handler(server, &logs_uri);

        httpd_uri_t sdtest_uri = { .uri = "/api/sdcard/test", .method = HTTP_GET, .handler = sdcard_test_handler };
        httpd_register_uri_handler(server, &sdtest_uri);

        httpd_uri_t favicon_uri = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get_handler };
        httpd_register_uri_handler(server, &favicon_uri);

        httpd_uri_t img_uri = { .uri = "/img/*", .method = HTTP_GET, .handler = image_get_handler };
        httpd_register_uri_handler(server, &img_uri);

        ESP_LOGI(TAG, "HTTP-Server gestartet und Endpunkte registriert.");
    } else {
        ESP_LOGE(TAG, "HTTP-Server konnte nicht gestartet werden.");
    }
}

static esp_err_t profiles_list_get_handler(httpd_req_t *req) {
    ProfileList_t *list = get_available_profiles();
    
    cJSON *root = cJSON_CreateArray();
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"allocation_failed\"}");
        return ESP_FAIL;
    }

    for (int i = 0; i < list->count; i++) {
        cJSON_AddItemToArray(root, cJSON_CreateString(list->names[i]));
    }

    const char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"allocation_failed\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void*)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}


static esp_err_t profile_activate_post_handler(httpd_req_t *req) {
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *name_item = cJSON_GetObjectItem(root, "profile_name");
        if (name_item && name_item->valuestring) {
            
            load_and_activate_profile(name_item->valuestring); 
        }
        cJSON_Delete(root);
    }
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

