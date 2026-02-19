#include "esp_log.h"
#include "bmx280.h"
#include "bmx280_sensor.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmx280_sensor";
static bmx280_t *bmx280;

/* ---- Initialization -----------------------------------------------------  */

esp_err_t bmx280_sensor_init(void)
{
    bmx280 = bmx280_create_master(display_get_i2c_bus());
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
    /* Start in sleep mode; sensor_task triggers forced reads on demand */
    ESP_ERROR_CHECK(bmx280_setMode(bmx280, BMX280_MODE_SLEEP));

    return ESP_OK;
}

bmx280_t *bmx280_sensor_get_handle(void)
{
    return bmx280;
}
