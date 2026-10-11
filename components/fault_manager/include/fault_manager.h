
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    FAULT_NONE = 0,
    FAULT_OVERCURRENT,
    FAULT_OVERTEMPERATURE,
    FAULT_SENSOR_ERROR
} fault_code_t;

typedef struct {
    fault_code_t code;
    int64_t timestamp_us;
    uint32_t occurrence_count;
    bool active;
} fault_record_t;

esp_err_t fault_manager_init(void);

esp_err_t fault_manager_set(fault_code_t code);

esp_err_t fault_manager_clear(void);

fault_record_t fault_manager_get(void);
