#pragma once

#include <stdint.h>

/** Initialise the custom GATT service.  Call once before starting the host. */
int gatt_svc_init(void);

/** Attribute handle for the read/write characteristic (set after registration). */
extern uint16_t gatt_svc_chr_val_handle;

/** Return the timezone offset in quarter-hours from UTC. */
int8_t gatt_svc_get_tz_quarter_hours(void);
