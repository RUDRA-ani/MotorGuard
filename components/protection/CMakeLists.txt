
idf_component_register(
    SRCS "protection.c"
    INCLUDE_DIRS "include"
    REQUIRES
        motor_controller
        sensor_manager
        fault_manager
)
