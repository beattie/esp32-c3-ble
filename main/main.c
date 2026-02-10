#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"

#if 0
#include "bmx280_task.h"
#else
#include "sensor_task.h"
#endif
#include "display.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include "gatt_svc.h"
#include "battery.h"

static const char *TAG = "ble_app";

#define DEVICE_NAME "ESP32-C3-BLE"

/* ---- Forward declarations ------------------------------------------------ */

static void ble_app_on_sync(void);
static void ble_app_on_reset(int reason);
static int  gap_event_handler(struct ble_gap_event *event, void *arg);

/* ---- Advertising --------------------------------------------------------- */

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    }
}

/* ---- GAP event handler --------------------------------------------------- */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; handle=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.conn_handle);
        if (event->connect.status != 0) {
            /* Connection failed — resume advertising. */
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected; reason=%d",
                 event->disconnect.reason);
        start_advertising();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertising complete");
        start_advertising();
        break;

    default:
        break;
    }

    return 0;
}

/* ---- NimBLE host callbacks ----------------------------------------------- */

static void ble_app_on_sync(void)
{
    int rc;

    /* Use best available address type. */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    start_advertising();
    ESP_LOGI(TAG, "advertising started");
}

static void ble_app_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset; reason=%d", reason);
}

/* ---- Host task ----------------------------------------------------------- */

static void nimble_host_task(void *param)
{
    /* This function returns only when nimble_port_stop() is called. */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ---- app_main ------------------------------------------------------------ */

void app_main(void)
{
    int rc;

    /* Initialise NVS — required by the BT controller. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "starting %s", DEVICE_NAME);
    
    if (display_init() != ESP_OK) {
        ESP_LOGW(TAG, "Display not available, continuing without it");
    }

#if 0
    if (bmx280_task_init() != ESP_OK) {
        ESP_LOGW(TAG, "BMX280 sensor not available, continuing without it");
    }

    if (battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "Battery reading not available, continuing without it");
    } else {
		int voltage_mv = battery_get_voltage_mv();
		ESP_LOGI(TAG, "Battery voltage: %d mV", voltage_mv);
	}
#else
    if (sensor_task_init() != ESP_OK) {
        ESP_LOGW(TAG, "Sensor task initialization failed, continuing without it");
    }
#endif


    /* Initialise the NimBLE host stack. */
    rc = nimble_port_init();
    assert(rc == 0);

    /* Set the host callbacks. */
    ble_hs_cfg.sync_cb  = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;

    /* Set the device name used by the GAP service. */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    assert(rc == 0);

    /* Initialise the custom GATT service. */
    rc = gatt_svc_init();
    assert(rc == 0);

    /* Start the NimBLE host task. */
    nimble_port_freertos_init(nimble_host_task);
}
