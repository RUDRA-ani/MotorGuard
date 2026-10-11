
#include "protection.h"

#include "motor_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define CURRENT_TRIP_RAW 3000
#define TEMP_TRIP_RAW    3000

#define CURRENT_RESET_RAW 2800
#define TEMP_RESET_RAW    2800

static const char *TAG = "PROTECTION";
static SemaphoreHandle_t s_protection_mutex = NULL;

/* Must be called before protection tasks start. */
esp_err_t protection_init(void)
{
    if (s_protection_mutex != NULL) {
        return ESP_OK;
    }

    s_protection_mutex = xSemaphoreCreateMutex();

    if (s_protection_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create protection mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Protection initialized");
    return ESP_OK;
}

/* Internal helper: caller must hold the protection mutex. */
static esp_err_t report_fault_locked(fault_code_t code)
{
    if (code <= FAULT_NONE || code > FAULT_SENSOR_ERROR) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = fault_manager_set(code);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to record fault: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = motor_controller_trip_fault();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to trip motor: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGE(TAG, "Fault handled: code=%d", (int)code);
    return ESP_OK;
}

esp_err_t protection_report_fault(fault_code_t code)
{
    if (s_protection_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_protection_mutex, portMAX_DELAY);
    esp_err_t err = report_fault_locked(code);
    xSemaphoreGive(s_protection_mutex);

    return err;
}

protection_fault_t protection_check(
    const sensor_readings_t *readings)
{
    if (s_protection_mutex == NULL) {
        ESP_LOGE(TAG, "Protection is not initialized");
        return PROTECTION_SENSOR_ERROR;
    }

    xSemaphoreTake(s_protection_mutex, portMAX_DELAY);

    protection_fault_t result = PROTECTION_OK;
    esp_err_t err = ESP_OK;

    if (readings == NULL) {
        result = PROTECTION_SENSOR_ERROR;
        err = report_fault_locked(FAULT_SENSOR_ERROR);
    } else if (readings->current_raw >= CURRENT_TRIP_RAW) {
        result = PROTECTION_OVERCURRENT;
        err = report_fault_locked(FAULT_OVERCURRENT);
    } else if (readings->temperature_raw >= TEMP_TRIP_RAW) {
        result = PROTECTION_OVERTEMPERATURE;
        err = report_fault_locked(FAULT_OVERTEMPERATURE);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Protection operation failed: %s",
                 esp_err_to_name(err));
    }

    xSemaphoreGive(s_protection_mutex);
    return result;
}

esp_err_t protection_clear_fault(
    const sensor_readings_t *readings)
{
    if (s_protection_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (readings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_protection_mutex, portMAX_DELAY);

    if (readings->current_raw >= CURRENT_RESET_RAW ||
        readings->temperature_raw >= TEMP_RESET_RAW) {
        ESP_LOGW(TAG, "Reset rejected: readings are not safe");
        xSemaphoreGive(s_protection_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (motor_controller_get_state() != MOTOR_STATE_FAULT) {
        ESP_LOGW(TAG, "Reset rejected: no motor fault is active");
        xSemaphoreGive(s_protection_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * Clear the motor state first. The protection mutex
     * prevents other protection operations from interleaving.
     */
    esp_err_t err = motor_controller_clear_fault();

    if (err != ESP_OK) {
        xSemaphoreGive(s_protection_mutex);
        return err;
    }

    err = fault_manager_clear();

    if (err != ESP_OK) {
        /*
         * Keep the motor stopped in a fault state if the
         * fault record cannot be cleared consistently.
         */
        esp_err_t restore_err = motor_controller_trip_fault();

        if (restore_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore motor fault state");
        }

        xSemaphoreGive(s_protection_mutex);
        return err;
    }

    ESP_LOGI(TAG, "Fault reset successful; motor remains stopped");

    xSemaphoreGive(s_protection_mutex);
    return ESP_OK;
}
