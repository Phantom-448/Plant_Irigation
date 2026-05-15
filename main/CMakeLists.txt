idf_component_register(
    SRCS "main.c" "wlan.c" "webserver.c" "actor.c" "humid.c" "state.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver esp_hal_ana_conv nvs_flash esp_wifi esp_netif esp_event esp_system esp_driver_gpio esp_driver_tsens esp_driver_i2c
    EMBED_TXTFILES "index.html"
)