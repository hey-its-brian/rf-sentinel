idf_component_register(
    SRCS "radios.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_timer
)
# RadioLib is pulled in through idf_component.yml in this directory.
