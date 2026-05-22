idf_component_register(
    SRCS "profile_manager.c" "logger.c" "sd_storage.c" "main.c" "wlan.c" "webserver.c" "actor.c" "humid.c" "state.c" "timer.c" "temp.c" "dht22.c"
    INCLUDE_DIRS "."
    REQUIRES cjson fatfs sdmmc nvs_flash esp_wifi esp_http_server driver esp_timer
    EMBED_TXTFILES "index.html"
)