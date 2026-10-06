idf_component_register(
    SRCS "wifiscan.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi nvs_flash esp_netif esp_event
)
