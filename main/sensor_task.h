#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"

extern int32_t gatt_svc_battery_mv; // Battery voltage in millivolts   

esp_err_t sensor_task_init(void);
extern bool sensors_valid; // Flag indicating if sensor readings are valid

#endif /* SENSOR_TASK_H */