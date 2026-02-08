#ifndef BMX280_TASK_H
#define BMX280_TASK_H

#include "esp_err.h"

extern float gatt_svc_pressure;
extern float gatt_svc_temperature;
extern float gatt_svc_humidity;

esp_err_t bmx280_task_init(void);

#endif /* BMX280_TASK_H */
