#include "power.h"

#include "esp_log.h"
#include "esp_pm.h"

static const char *TAG = "power";
esp_err_t power_init(void)
{
    /* Enable automatic light sleep with BLE modem sleep */
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };

    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure power management: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Power management configured: max_freq=%d MHz, min_freq=%d MHz, light_sleep=%s",
             pm_config.max_freq_mhz, pm_config.min_freq_mhz,
             pm_config.light_sleep_enable ? "enabled" : "disabled");
    return ESP_OK;
}