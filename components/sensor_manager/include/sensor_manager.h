
#pragma once

#include "esp_err.h"

typedef struct {
    int current_raw;
    int temperature_raw;
} sensor_readings_t;

esp_err_t sensor_manager_init(void);
esp_err_t sensor_manager_read(sensor_readings_t *readings);
