idf_component_register(
    SRCS "main.c" "wlan.c" "webserver.c" "actor.c" "temp.c" "humid.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver esp_hal_ana_conv
    EMBED_TXTFILES "index.html"
)