
#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t system_monitor_init(void);

void system_monitor_task(void *pvParameters);

#endif // SYSTEM_MONITOR_H
