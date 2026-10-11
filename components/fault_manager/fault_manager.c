
#include "fault_manager.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "FAULT_MANAGER";

static fault_record_t current_fault = {
    .code = FAULT_NONE,
    .timestamp_us = 0,
    .occurrence_count = 0,
    .active = false
};

static SemaphoreHandle_t fault_mutex = NULL;

esp_err_t fault_manager_init(void)
{
    if (fault_mutex == NULL) {
        fault_mutex = xSemaphoreCreateMutex();

        if (fault_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create fault mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(fault_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    current_fault = (fault_record_t) {
        .code = FAULT_NONE,
        .timestamp_us = 0,
        .occurrence_count = 0,
        .active = false
    };

    xSemaphoreGive(fault_mutex);

    ESP_LOGI(TAG, "Fault manager initialized");
    return ESP_OK;
}

esp_err_t fault_manager_set(fault_code_t code)
{
    if (code <= FAULT_NONE || code > FAULT_SENSOR_ERROR) {
        return ESP_ERR_INVALID_ARG;
    }

    if (fault_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(fault_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (current_fault.active && current_fault.code == code) {
        xSemaphoreGive(fault_mutex);
        return ESP_OK;
    }

    current_fault.code = code;
    current_fault.timestamp_us = esp_timer_get_time();
    current_fault.occurrence_count++;
    current_fault.active = true;

    uint32_t occurrence_count = current_fault.occurrence_count;

    xSemaphoreGive(fault_mutex);

    ESP_LOGE(TAG, "Fault latched: code=%d, count=%lu",
             (int)code,
             (unsigned long)occurrence_count);

    return ESP_OK;
}

esp_err_t fault_manager_clear(void)
{
    if (fault_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(fault_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (!current_fault.active) {
        xSemaphoreGive(fault_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    fault_code_t cleared_code = current_fault.code;

    current_fault.active = false;
    current_fault.code = FAULT_NONE;

    xSemaphoreGive(fault_mutex);

    ESP_LOGW(TAG, "Fault cleared: code=%d", (int)cleared_code);

    return ESP_OK;
}

fault_record_t fault_manager_get(void)
{
    fault_record_t result = {
        .code = FAULT_NONE,
        .timestamp_us = 0,
        .occurrence_count = 0,
        .active = false
    };

    if (fault_mutex == NULL) {
        return result;
    }

    if (xSemaphoreTake(fault_mutex, portMAX_DELAY) != pdTRUE) {
        return result;
    }

    result = current_fault;

    xSemaphoreGive(fault_mutex);

    return result;
}
