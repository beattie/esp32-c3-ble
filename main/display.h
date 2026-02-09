#ifndef DISPLAY_H
#define DISPLAY_H

#include <esp_err.h>
#include "driver/i2c_master.h"

esp_err_t display_init(void);
i2c_master_bus_handle_t display_get_i2c_bus(void);

#endif  /* DISPLAY_H */