
#include "motor_controller.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MOTOR_OUTPUT_GPIO GPIO_NUM_18
#define FAULT_LED_GPIO    GPIO_NUM_19

static const char *TAG = "MOTOR_CTRL";

static motor_state_t s_state = MOTOR_STATE_STOPPED;
static SemaphoreHandle_t motor_mutex = NULL;


/* Update both outputs while holding the motor mutex. */
static void update_outputs_locked(void)
{
    const bool motor_on = (s_state == MOTOR_STATE_RUNNING);
    const bool fault_on = (s_state == MOTOR_STATE_FAULT);

    gpio_set_level(MOTOR_OUTPUT_GPIO, motor_on ? 1 : 0);
    gpio_set_level(FAULT_LED_GPIO, fault_on ? 1 : 0);
}


esp_err_t motor_controller_init(void)
{
    if (motor_mutex == NULL) {
        motor_mutex = xSemaphoreCreateMutex();

        if (motor_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create motor mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    const uint64_t output_mask =
        (1ULL << MOTOR_OUTPUT_GPIO) |
        (1ULL << FAULT_LED_GPIO);

    gpio_config_t config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);

    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    s_state = MOTOR_STATE_STOPPED;
    update_outputs_locked();

    xSemaphoreGive(motor_mutex);

    ESP_LOGI(TAG, "Motor controller initialized");
    return ESP_OK;
}


esp_err_t motor_controller_start(void)
{
    if (motor_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    if (s_state == MOTOR_STATE_FAULT) {
        xSemaphoreGive(motor_mutex);
        ESP_LOGW(TAG, "Start rejected: fault is active");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == MOTOR_STATE_RUNNING) {
        xSemaphoreGive(motor_mutex);
        return ESP_OK;
    }

    s_state = MOTOR_STATE_RUNNING;
    update_outputs_locked();

    xSemaphoreGive(motor_mutex);

    ESP_LOGI(TAG, "Motor started");
    return ESP_OK;
}


esp_err_t motor_controller_stop(void)
{
    if (motor_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    if (s_state == MOTOR_STATE_FAULT) {
        xSemaphoreGive(motor_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    s_state = MOTOR_STATE_STOPPED;
    update_outputs_locked();

    xSemaphoreGive(motor_mutex);

    ESP_LOGI(TAG, "Motor stopped");
    return ESP_OK;
}


esp_err_t motor_controller_trip_fault(void)
{
    if (motor_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    s_state = MOTOR_STATE_FAULT;
    update_outputs_locked();

    xSemaphoreGive(motor_mutex);

    ESP_LOGE(TAG, "FAULT LATCHED: motor output disabled");
    return ESP_OK;
}


esp_err_t motor_controller_clear_fault(void)
{
    if (motor_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    if (s_state != MOTOR_STATE_FAULT) {
        xSemaphoreGive(motor_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    s_state = MOTOR_STATE_STOPPED;
    update_outputs_locked();

    xSemaphoreGive(motor_mutex);

    ESP_LOGW(TAG, "Fault cleared; motor remains stopped");
    return ESP_OK;
}


motor_state_t motor_controller_get_state(void)
{
    if (motor_mutex == NULL) {
        return MOTOR_STATE_STOPPED;
    }

    xSemaphoreTake(motor_mutex, portMAX_DELAY);

    motor_state_t state = s_state;

    xSemaphoreGive(motor_mutex);

    return state;
}
