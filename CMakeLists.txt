
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "components/motor_controller"
    "components/sensor_manager"
    "components/protection"
    "components/fault_manager"
    "components/system_monitor"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(motor_guard)
