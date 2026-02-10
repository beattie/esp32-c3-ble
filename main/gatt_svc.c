#include "gatt_svc.h"

#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "gatt_svc";

/* ---- Custom 128-bit UUIDs ------------------------------------------------
 *
 * Service:        deadbeef-1000-2000-3000-aabbccddeeff
 * Characteristic: deadbeef-1001-2000-3000-aabbccddeeff
 * Pressure:       deadbeef-1002-2000-3000-aabbccddeeff
 * Temperature:    deadbeef-1003-2000-3000-aabbccddeeff
 * Humidity:       deadbeef-1004-2000-3000-aabbccddeeff
 * Time:           deadbeef-1005-2000-3000-aabbccddeeff
 * Timezone:       deadbeef-1006-2000-3000-aabbccddeeff
 *
 * NimBLE stores UUIDs in little-endian byte order.
 */
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x00, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x01, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_time_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x05, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_tz_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x06, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_press_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x02, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_temp_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x03, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_hum_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x04, 0x10, 0xef, 0xbe, 0xad, 0xde);

/* ---- Characteristic value storage ---------------------------------------- */

#define CHR_VAL_MAX_LEN 64

static uint8_t chr_val[CHR_VAL_MAX_LEN];
static uint16_t chr_val_len;

uint16_t gatt_svc_chr_val_handle;

/* ---- BMX Sensor values --------------------------------------------------- */
float gatt_svc_pressure;
float gatt_svc_temperature;
float gatt_svc_humidity;

/* ---- Battery level ------------------------------------------------------- */

uint32_t gatt_svc_battery_mv;

/* ---- Timezone storage ---------------------------------------------------- */
/* Timezone offset in quarter-hours from UTC (int8_t, e.g. -20 = UTC-5, +22 = UTC+5:30) */
static int8_t tz_quarter_hours;

/* ---- Access callback ----------------------------------------------------- */

static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        rc = os_mbuf_append(ctxt->om, chr_val, chr_val_len);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len > CHR_VAL_MAX_LEN) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        rc = ble_hs_mbuf_to_flat(ctxt->om, chr_val, sizeof(chr_val),
                                 &chr_val_len);
        return rc == 0 ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ---- Time access callback ------------------------------------------------ */

static int time_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t now = (int64_t)tv.tv_sec;
        rc = os_mbuf_append(ctxt->om, &now, sizeof(now));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len != sizeof(int64_t)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        int64_t ts;
        uint16_t flat_len;
        rc = ble_hs_mbuf_to_flat(ctxt->om, &ts, sizeof(ts), &flat_len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        struct timeval tv = { .tv_sec = (time_t)ts, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "system time set to %lld", (long long)ts);
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ---- Sensor access callback ---------------------------------------------- */

static int sensor_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    float val;
    int rc;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (ble_uuid_cmp(uuid, &chr_press_uuid.u) == 0) {
        val = gatt_svc_pressure;
    } else if (ble_uuid_cmp(uuid, &chr_temp_uuid.u) == 0) {
        val = gatt_svc_temperature;
    } else if (ble_uuid_cmp(uuid, &chr_hum_uuid.u) == 0) {
        val = gatt_svc_humidity;
    } else {
        return BLE_ATT_ERR_UNLIKELY;
    }

    rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* ---- Timezone access callback -------------------------------------------- */

static int tz_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        rc = os_mbuf_append(ctxt->om, &tz_quarter_hours, sizeof(tz_quarter_hours));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len != sizeof(int8_t)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        int8_t val;
        uint16_t flat_len;
        rc = ble_hs_mbuf_to_flat(ctxt->om, &val, sizeof(val), &flat_len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        tz_quarter_hours = val;
        ESP_LOGI(TAG, "timezone set to %+d quarter-hours (UTC%+d:%02d)",
                 val, val / 4, abs(val % 4) * 15);
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ---- Service definition -------------------------------------------------- */

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &chr_uuid.u,
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &gatt_svc_chr_val_handle,
            },
            {
                 .uuid = &chr_press_uuid.u,
                 .access_cb = sensor_access_cb,
                 .flags = BLE_GATT_CHR_F_READ,
            },
            {
                 .uuid = &chr_temp_uuid.u,
                 .access_cb = sensor_access_cb,
                 .flags = BLE_GATT_CHR_F_READ,
            },
            {
                 .uuid = &chr_hum_uuid.u,
                 .access_cb = sensor_access_cb,
                 .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = &chr_time_uuid.u,
                .access_cb = time_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = &chr_tz_uuid.u,
                .access_cb = tz_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {0}, /* terminator */
        },
    },
    {0}, /* terminator */
};

/* ---- Public API ---------------------------------------------------------- */

int8_t gatt_svc_get_tz_quarter_hours(void)
{
    return tz_quarter_hours;
}

int gatt_svc_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    return rc;
}
