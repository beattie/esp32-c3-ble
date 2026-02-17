#include "battery.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_3
#define BUTTON_ADC_CHANNEL ADC_CHANNEL_4

// ADC Attenuation
#define ADC_ATTENUATION ADC_ATTEN_DB_12 // Changed from ADC_ATTEN_DB_11 to ADC_ATTEN_DB_12

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc_cali_handle = NULL;

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        calibrated = true;
    }

    *out_handle = handle;
    return calibrated;
}

esp_err_t battery_init(void)
{
    // ADC1 Init
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // ADC1 Config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTENUATION,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BUTTON_ADC_CHANNEL, &config));

    // ADC calibration
    if (!adc_calibration_init(ADC_UNIT, ADC_ATTENUATION, &adc_cali_handle)) {
        ESP_LOGE(TAG, "ADC calibration not enabled");
    }

    return ESP_OK;
}

int battery_get_voltage_mv(void)
{
    int raw_val;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_val));
    
    int voltage_mv = 0;
    if (adc_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv));
    } else {
        // Fallback to linear conversion if calibration is not available
        // This is not accurate. For better accuracy, use esp_adc_cal.
        voltage_mv = raw_val * 2500 / 4095;
        ESP_LOGW(TAG, "ADC calibration not enabled, using linear conversion. Voltage: %d mV", voltage_mv);
    }
    return voltage_mv;
}

int button_read_mv(void)
{
    int raw_val;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BUTTON_ADC_CHANNEL, &raw_val));
    int voltage_mv = 0;
    if (adc_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv));
    }
    return voltage_mv;
}
