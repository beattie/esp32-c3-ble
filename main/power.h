#ifndef POWER_H
#define POWER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize automatic light sleep with BLE modem sleep.
 * Call after NimBLE host is started.
 */

esp_err_t power_init(void);

#endif