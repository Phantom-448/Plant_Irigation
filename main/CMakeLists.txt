idf_component_register(
    SRCS "profile_manager.c" "logger.c" "sd_storage.c" "main.c" "wlan.c" "webserver.c" "actor.c" "humid.c" "state.c" "timer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server cjson driver esp_hal_ana_conv nvs_flash esp_wifi esp_netif esp_event esp_system esp_timer esp_driver_gpio esp_driver_tsens esp_driver_i2c
    EMBED_TXTFILES "index.html"
)