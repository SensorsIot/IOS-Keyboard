set(SRCS
    "main.c"
    "wifi_manager.c"
    "captive_portal.c"
    "debug_server.c"
    "ota_handler.c"
    "usb_hid.c"
    "command_parser.c"
    "ble_gatt.c"
    "keyboard_layout.c"
)

set(REQUIRES
    esp_http_server
    esp_https_ota
    nvs_flash
    esp_wifi
    esp_netif
    esp_event
    esp_timer
    esp_app_format
    app_update
    bt
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES ${REQUIRES}
)
