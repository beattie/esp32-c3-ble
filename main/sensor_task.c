#include <stdio.h>
#include "esp_pm.h"

#include "esp_log.h"
#include "sensor_task.h"
#include "bmx280.h"
#include "bmx280_sensor.h"
#include "battery.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensor_task";

/* ---- Sensors validity flags --------------------------------------------- */

bool sensors_valid = false;

/* ---- Sensor reading task ------------------------------------------------ */

static void sensor_task(void *param)
{
    while (1) {
        /* Read sensor data and update GATT service variables */
        float temperature, pressure, humidity;
        bmx280_t *bmx = bmx280_sensor_get_handle();
        if (!bmx) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }

        do {
            vTaskDelay(pdMS_TO_TICKS(100));
        } while (bmx280_isSampling(bmx));

        esp_err_t err = bmx280_readoutFloat(bmx, &temperature, &pressure, &humidity);
        if (err == ESP_OK) {
            gatt_svc_temperature = temperature;
            gatt_svc_pressure = pressure;
            gatt_svc_humidity = humidity;
            ESP_LOGI(TAG, "Temperature: %.2f °C, Pressure: %.2f hPa, Humidity: %.2f %%", temperature, pressure / 100.0, humidity);
            
            sensors_valid = true; // Mark sensor readings as valid
        } else {
            ESP_LOGE(TAG, "Failed to read from bmx280: %s", esp_err_to_name(err));
        }
        
        gatt_svc_battery_mv = battery_get_voltage_mv(); // Update battery 
                                                        // voltage reading
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ---- Initialization ----------------------------------------------------- */

esp_err_t sensor_task_init(void)
{
    bmx280_sensor_init(); // Initialize the sensor (e.g., I2C setup, sensor config)
    battery_init(); // Initialize battery reading (e.g., ADC setup)
    
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}
