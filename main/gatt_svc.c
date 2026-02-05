#include "gatt_svc.h"

#include <string.h>

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* ---- Custom 128-bit UUIDs ------------------------------------------------
 *
 * Service:        deadbeef-1000-2000-3000-aabbccddeeff
 * Characteristic: deadbeef-1001-2000-3000-aabbccddeeff
 *
 * NimBLE stores UUIDs in little-endian byte order.
 */
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x00, 0x10, 0xef, 0xbe, 0xad, 0xde);

static const ble_uuid128_t chr_uuid =
    BLE_UUID128_INIT(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x30,
                     0x00, 0x20, 0x01, 0x10, 0xef, 0xbe, 0xad, 0xde);

/* ---- Characteristic value storage ---------------------------------------- */

#define CHR_VAL_MAX_LEN 64

static uint8_t chr_val[CHR_VAL_MAX_LEN];
static uint16_t chr_val_len;

uint16_t gatt_svc_chr_val_handle;

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
            {0}, /* terminator */
        },
    },
    {0}, /* terminator */
};

/* ---- Public API ---------------------------------------------------------- */

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
