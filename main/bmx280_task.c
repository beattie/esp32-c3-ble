#include "esp_log.h"
#include "bmx280.h"
#include "bmx280_task.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmx280_task";

/* ---- Reading task -------------------------------------------------------  */

static void bmx280_task(void *param)
{
    bmx280_t *bmx280 = (bmx280_t *)param;

    while (1) {
        /* Wait for measurement to complete */
        do {
            vTaskDelay(pdMS_TO_TICKS(100));
        } while (bmx280_isSampling(bmx280));

        esp_err_t err = bmx280_readoutFloat(bmx280, &gatt_svc_temperature, &gatt_svc_pressure, &gatt_svc_humidity);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f °C, Pressure: %.2f hPa, Humidity: %.2f %%", gatt_svc_temperature, gatt_svc_pressure / 100.0, gatt_svc_humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read from bmx280: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- Initialization -----------------------------------------------------  */

esp_err_t bmx280_task_init(void)
{
    bmx280_t *bmx280 = bmx280_create_master(display_get_i2c_bus());
    if (bmx280 == NULL) {
        ESP_LOGE(TAG, "Failed to create bmx280 instance");
        return ESP_FAIL;
    }

    esp_err_t err = bmx280_init(bmx280);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bmx280: %s", esp_err_to_name(err));
        bmx280_close(bmx280);
        return err;
    }

    bmx280_config_t config = BMX280_DEFAULT_CONFIG;
    ESP_ERROR_CHECK(bmx280_configure(bmx280, &config));
    ESP_ERROR_CHECK(bmx280_setMode(bmx280, BMX280_MODE_CYCLE));

    xTaskCreate(bmx280_task, "bmx280_task", 4096, bmx280, 5, NULL);
    return ESP_OK;
}
