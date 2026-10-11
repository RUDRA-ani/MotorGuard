
#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_FAULT
} motor_state_t;

esp_err_t motor_controller_init(void);

esp_err_t motor_controller_start(void);
esp_err_t motor_controller_stop(void);

esp_err_t motor_controller_trip_fault(void);
esp_err_t motor_controller_clear_fault(void);

motor_state_t motor_controller_get_state(void);
