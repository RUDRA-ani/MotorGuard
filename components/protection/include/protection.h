
#pragma once

#include "esp_err.h"
#include "sensor_manager.h"
#include "fault_manager.h"

typedef enum {
    PROTECTION_OK = 0,
    PROTECTION_OVERCURRENT,
    PROTECTION_OVERTEMPERATURE,
    PROTECTION_SENSOR_ERROR
} protection_fault_t;

esp_err_t protection_init(void);

protection_fault_t protection_check(
    const sensor_readings_t *readings);

esp_err_t protection_report_fault(
    fault_code_t code);

esp_err_t protection_clear_fault(
    const sensor_readings_t *readings);
