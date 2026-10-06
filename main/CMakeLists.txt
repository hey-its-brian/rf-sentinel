idf_component_register(
    SRCS "main.cpp" "proto.cpp" "baseline.cpp"
    INCLUDE_DIRS "."
    REQUIRES radios gps wifiscan esp_driver_uart esp_timer
)
