
#include "system_monitor.h"

#include <inttypes.h>
#include <stdint.h>

#include "motor_controller.h"
#include "fault_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MONITOR_INTERVAL_MS 1000

static const char *TAG = "SYSTEM_MONITOR";

static const char *motor_state_to_string(motor_state_t state)
{
    switch (state) {
    case MOTOR_STATE_STOPPED:
        return "STOPPED";
    case MOTOR_STATE_RUNNING:
        return "RUNNING";
    case MOTOR_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static const char *fault_code_to_string(fault_code_t code)
{
    switch (code) {
    case FAULT_NONE:
        return "NONE";
    case FAULT_OVERCURRENT:
        return "OVERCURRENT";
    case FAULT_OVERTEMPERATURE:
        return "OVERTEMPERATURE";
    case FAULT_SENSOR_ERROR:
        return "SENSOR_ERROR";
    default:
        return "UNKNOWN";
    }
}

esp_err_t system_monitor_init(void)
{
    ESP_LOGI(TAG, "System monitor initialized");
    return ESP_OK;
}

void system_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(MONITOR_INTERVAL_MS);

    while (true) {
        motor_state_t motor_state = motor_controller_get_state();
        fault_record_t fault = fault_manager_get();

        ESP_LOGI(TAG, "========== System Status ==========");
        ESP_LOGI(TAG, "Motor state : %s",
                 motor_state_to_string(motor_state));
        ESP_LOGI(TAG, "Fault code  : %s",
                 fault_code_to_string(fault.code));
        ESP_LOGI(TAG, "Fault active: %s",
                 fault.active ? "YES" : "NO");
        ESP_LOGI(TAG, "Occurrences : %" PRIu32,
                 (uint32_t)fault.occurrence_count);

        if (fault.timestamp_us > 0) {
            ESP_LOGI(TAG, "Last fault  : %" PRId64 " us since boot",
                     (int64_t)fault.timestamp_us);
        } else {
            ESP_LOGI(TAG, "Last fault  : No recorded fault");
        }

        TaskHandle_t sensor_task = xTaskGetHandle("sensor_monitor");

        if (sensor_task != NULL) {
            ESP_LOGI(TAG, "Stack remaining: monitor=%u words, sensor=%u words",
                     (unsigned int)uxTaskGetStackHighWaterMark(NULL),
                     (unsigned int)uxTaskGetStackHighWaterMark(sensor_task));
        } else {
            ESP_LOGW(TAG, "Sensor monitor task not found");
            ESP_LOGI(TAG, "Stack remaining: monitor=%u words",
                     (unsigned int)uxTaskGetStackHighWaterMark(NULL));
        }

        ESP_LOGI(TAG, "===================================");

        vTaskDelayUntil(&last_wake_time, interval);
    }
}
