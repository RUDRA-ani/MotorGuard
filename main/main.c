
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "motor_controller.h"
#include "sensor_manager.h"
#include "protection.h"
#include "fault_manager.h"
#include "system_monitor.h"

#define BUTTON_GPIO             GPIO_NUM_23
#define POLL_INTERVAL_MS        10
#define DEBOUNCE_SAMPLES        5
#define LONG_PRESS_THRESHOLD_MS 2000
#define SENSOR_INTERVAL_MS      100
#define LOG_INTERVAL_MS         1000

#define MONITOR_TASK_STACK_SIZE 3072
#define MONITOR_TASK_PRIORITY   2

#define SENSOR_TASK_STACK_SIZE  4096
#define SENSOR_TASK_PRIORITY    5
#define SENSOR_STALE_TIMEOUT_MS 500

static const char *TAG = "MOTOR_GUARD";

typedef struct {
    sensor_readings_t readings;
    bool valid;
    int64_t timestamp_ms;
} sensor_snapshot_t;

/* Configure the button input. */
static esp_err_t button_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}


/* Handle short-press and long-press actions. */
static void handle_button_press(
    int64_t duration_ms,
    const sensor_readings_t *readings,
    
    bool sensor_data_valid)
{
    motor_state_t state = motor_controller_get_state();

    if (duration_ms >= LONG_PRESS_THRESHOLD_MS) {
        if (state == MOTOR_STATE_FAULT) {
            if (!sensor_data_valid) {
                ESP_LOGW(TAG,
                         "Reset rejected: sensor data unavailable");
                return;
            }

            esp_err_t err = protection_clear_fault(readings);

            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Fault reset rejected: %s",
                         esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "Fault reset successful");
            }
        } else {

ESP_LOGW(TAG, "Injecting test fault");

esp_err_t err =
    protection_report_fault(FAULT_SENSOR_ERROR);

if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to inject test fault: %s",
             esp_err_to_name(err));
}

        }

        return;
    }

    if (state == MOTOR_STATE_FAULT) {
        ESP_LOGW(TAG, "Start rejected: fault active");
        return;
    }

    esp_err_t err;

    if (state == MOTOR_STATE_RUNNING) {
        err = motor_controller_stop();
    } else {
        err = motor_controller_start();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Motor command failed: %s",
                 esp_err_to_name(err));
    }
}


/* Sample sensors and execute protection checks independently. */
static void sensor_monitor_task(void *pvParameters)
{
    QueueHandle_t sensor_queue = (QueueHandle_t)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(SENSOR_INTERVAL_MS);

    sensor_snapshot_t snapshot = {0};

    while (true) {
        esp_err_t err = sensor_manager_read(&snapshot.readings);

        if (err == ESP_OK) {
            snapshot.valid = true;
            protection_check(&snapshot.readings);
        } 
else {
    snapshot.valid = false;

    ESP_LOGE(TAG, "Sensor read failed: %s",
             esp_err_to_name(err));

    esp_err_t fault_err =
        protection_report_fault(FAULT_SENSOR_ERROR);

    if (fault_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to activate sensor protection: %s",
                 esp_err_to_name(fault_err));
    }
}


/* Timestamp every publication, including failed sensor reads. */
snapshot.timestamp_ms = esp_timer_get_time() / 1000;

if (xQueueOverwrite(sensor_queue, &snapshot) != pdPASS) {
    ESP_LOGE(TAG, "Failed to publish sensor snapshot");
}
        vTaskDelayUntil(&last_wake_time, interval);
    }
}


/* Application entry point. */
void app_main(void)
{
    printf("\n=== MOTOR GUARD ===\n");
    fflush(stdout);

    ESP_LOGI(TAG, "Motor Guard starting");

    ESP_ERROR_CHECK(motor_controller_init());
    ESP_ERROR_CHECK(fault_manager_init());
    ESP_ERROR_CHECK(button_init());
    ESP_ERROR_CHECK(protection_init());
    ESP_ERROR_CHECK(sensor_manager_init());
    ESP_ERROR_CHECK(system_monitor_init());

    /* One-element queue stores the most recent sensor snapshot. */
    QueueHandle_t sensor_queue = xQueueCreate(
        1,
        sizeof(sensor_snapshot_t)
    );

    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor queue");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    /* Start the sensor monitoring task. */
    BaseType_t task_result = xTaskCreate(
        sensor_monitor_task,
        "sensor_monitor",
        SENSOR_TASK_STACK_SIZE,
        sensor_queue,
        SENSOR_TASK_PRIORITY,
        NULL
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor monitor task");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    /* Start the system monitoring task. */
    task_result = xTaskCreate(
        system_monitor_task,
        "system_monitor",
        MONITOR_TASK_STACK_SIZE,
        NULL,
        MONITOR_TASK_PRIORITY,
        NULL
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    bool stable_pressed = false;
    bool candidate_pressed = false;
    uint8_t debounce_count = 0;

    int64_t press_start_us = 0;
    int64_t last_log_ms = 0;

bool sensor_data_valid = false;
bool stale_fault_reported = false;

sensor_readings_t readings = {0};
sensor_snapshot_t snapshot = {0};

    ESP_LOGI(TAG, "Ready: short press toggles motor");
    ESP_LOGI(TAG, "Long press injects or resets a fault");
    ESP_LOGI(TAG,
             "GPIO34=current input, GPIO35=temperature input");

  
    while (true) {
        /* Get the latest sensor snapshot without blocking. */
        if (xQueuePeek(sensor_queue, &snapshot, 0) == pdTRUE) {
            readings = snapshot.readings;
        }

        int64_t now_ms = esp_timer_get_time() / 1000;

        bool snapshot_fresh =
            snapshot.timestamp_ms > 0 &&
            (now_ms - snapshot.timestamp_ms) <=
                SENSOR_STALE_TIMEOUT_MS;

        sensor_data_valid = snapshot_fresh && snapshot.valid;

        /* Detect a stalled sensor task. */
        bool sensor_timed_out =
            !snapshot_fresh &&
            (snapshot.timestamp_ms == 0
                ? now_ms > SENSOR_STALE_TIMEOUT_MS
                : (now_ms - snapshot.timestamp_ms) >
                    SENSOR_STALE_TIMEOUT_MS);

        if (sensor_timed_out && !stale_fault_reported) {
            ESP_LOGE(TAG, "Sensor task stopped updating snapshots");

            if (motor_controller_get_state() != MOTOR_STATE_FAULT) {
                esp_err_t err =
                    protection_report_fault(FAULT_SENSOR_ERROR);

                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to latch stale-sensor fault: %s",
                             esp_err_to_name(err));
                }
            }

            stale_fault_reported = true;
        } else if (snapshot_fresh &&
                   motor_controller_get_state() != MOTOR_STATE_FAULT) {
            stale_fault_reported = false;
        }

        /* Debounce the active-low button. */
        bool raw_pressed = gpio_get_level(BUTTON_GPIO) == 0;

        if (raw_pressed == candidate_pressed) {
            if (debounce_count < DEBOUNCE_SAMPLES) {
                debounce_count++;
            }
        } else {
            candidate_pressed = raw_pressed;
            debounce_count = 1;
        }

        if (debounce_count >= DEBOUNCE_SAMPLES &&
            stable_pressed != candidate_pressed) {

            stable_pressed = candidate_pressed;

            if (stable_pressed) {
                press_start_us = esp_timer_get_time();
            } else {
                int64_t duration_ms =
                    (esp_timer_get_time() - press_start_us) / 1000;

                handle_button_press(
                    duration_ms,
                    &readings,
                    sensor_data_valid
                );
            }
        }

        /* Periodic application logging. */
        if (now_ms - last_log_ms >= LOG_INTERVAL_MS) {
            last_log_ms = now_ms;

            fault_record_t fault = fault_manager_get();

            ESP_LOGI(TAG,
                     "Current ADC=%d, Temperature ADC=%d, "
                     "Motor=%d, Fault=%d, Active=%d, SensorValid=%d",
                     readings.current_raw,
                     readings.temperature_raw,
                     (int)motor_controller_get_state(),
                     (int)fault.code,
                     (int)fault.active,
                     (int)sensor_data_valid);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
