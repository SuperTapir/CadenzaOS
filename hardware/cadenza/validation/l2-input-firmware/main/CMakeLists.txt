idf_component_register(
    SRCS "main.c" "input_logic.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_timer
)
