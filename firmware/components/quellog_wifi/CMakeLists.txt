idf_component_register(
    SRCS
        "dns_server.cc"
        "ssid_manager.cc"
        "wifi_configuration_ap.cc"
        "wifi_manager.cc"
        "wifi_station.cc"
    INCLUDE_DIRS
        "include"
    EMBED_TXTFILES
        "${QUELLOG_WEB_UI_INDEX}"
        "${QUELLOG_WEB_UI_DONE}"
    REQUIRES
        esp_event
        esp_http_server
        esp_netif
        esp_timer
        esp_wifi
        json
        nvs_flash
)

add_dependencies(${COMPONENT_LIB} quellog_web_ui_build)
