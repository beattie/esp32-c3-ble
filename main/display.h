#ifndef DISPLAY_H
#define DISPLAY_H

#include <esp_err.h>
#include "driver/i2c_master.h"

esp_err_t display_init(void);
i2c_master_bus_handle_t display_get_i2c_bus(void);
extern uint8_t gatt_svc_display_mode;

enum {
    DISPLAY_MODE_NORMAL = 0,    // Normal display mode with sensor readings ON
    DISPLAY_MODE_BUTTON = 1,    // Display shows for 5 seconds after button
    DISPLAY_MODE_BLANK = 2,     // Display always blanked
};

#endif  /* DISPLAY_H */