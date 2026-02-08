#ifndef OLED_H
#define OLED_H

#include "esp_err.h"

/** Initialize the SSD1306 OLED display. I2C bus must already be initialized. */
esp_err_t oled_init(void);

/** Display current system time (24hr HH:MM:SS) on the OLED. */
void oled_show_time(void);

#endif
