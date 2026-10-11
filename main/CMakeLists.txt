
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        motor_controller
        sensor_manager
        protection
        fault_manager
        system_monitor
        esp_timer
        esp_driver_gpio
)
