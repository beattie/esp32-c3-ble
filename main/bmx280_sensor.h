#ifndef BMX280_SENSOR_H
#define BMX280_SENSOR_H

#include "esp_err.h"
#include "bmx280.h"

extern float gatt_svc_pressure;
extern float gatt_svc_temperature;
extern float gatt_svc_humidity;

esp_err_t bmx280_sensor_init(void);
bmx280_t *bmx280_sensor_get_handle(void);

#endif /* BMX280_SENSOR_H */
