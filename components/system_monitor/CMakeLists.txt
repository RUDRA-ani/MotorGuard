
idf_component_register(
    SRCS "system_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES motor_controller fault_manager
)
