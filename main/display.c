#include "display.h"
#include "bmx280_sensor.h"
#include "sensor_task.h"
#include "gatt_svc.h"
#include "button.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA_GPIO   5
#define I2C_SCL_GPIO   6
#define LCD_H_RES      128
#define LCD_V_RES      64
#define LCD_I2C_ADDR   0x3C

static const char *TAG = "display";
static esp_lcd_panel_handle_t panel;
static i2c_master_bus_handle_t i2c_bus;

/* ---- 8x8 font (column-major, LSB = top pixel) -------------------------- */

enum {
    GLYPH_COLON = 10,
    GLYPH_DOT,
    GLYPH_DEG,
    GLYPH_F,
    GLYPH_C,
    GLYPH_PCT,
    GLYPH_hP,
    GLYPH_P,
    GLYPH_a,
    GLYPH_m,
    GLYPH_V,
    GLYPH_R,
    GLYPH_H,
};

static const uint8_t font[][8] = {
    { 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00, 0x00 }, /* 0 */
    { 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, 0x00, 0x00 }, /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00, 0x00 }, /* 2 */
    { 0x21, 0x41, 0x45, 0x4B, 0x31, 0x00, 0x00, 0x00 }, /* 3 */
    { 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00, 0x00 }, /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00, 0x00 }, /* 5 */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, 0x00, 0x00 }, /* 6 */
    { 0x01, 0x71, 0x09, 0x05, 0x03, 0x00, 0x00, 0x00 }, /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00 }, /* 8 */
    { 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00, 0x00 }, /* 9 */
    { 0x00, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* : */
    { 0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* . */
    { 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00 }, /* DEG */
    { 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00, 0x00, 0x00 }, /* F */
    { 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x00, 0x00 }, /* C */
    { 0x23, 0x13, 0x08, 0x64, 0x62, 0x00, 0x00, 0x00 }, /* % */
    { 0x7F, 0x08, 0x04, 0x04, 0x7f, 0x09, 0x09, 0x06 }, /* hP ligature*/
    { 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00 }, /* P */
    { 0x20, 0x54, 0x54, 0x54, 0x78, 0x00, 0x00, 0x00 }, /* a */
    { 0x7C, 0x04, 0x18, 0x04, 0x78, 0x00, 0x00, 0x00 }, /* m */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00, 0x00, 0x00 }, /* V */
    { 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00, 0x00, 0x00 }, /* R */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, 0x00 }, /* H */
};

/* ---- Framebuffer -------------------------------------------------------- */

static uint8_t fb[8][LCD_H_RES]; /* 8 pages × 128 columns */

static void fb_clear(void)
{
    memset(fb, 0, sizeof(fb));
}

static void fb_draw_glyph(int page, int col, int glyph_idx)
{
    const uint8_t *g = font[glyph_idx];
    for (int x = 0; x < 8 && (col + x) < LCD_H_RES; x++)
        fb[page][col + x] = g[x];
}

static void fb_flush(void)
{
    esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_H_RES, LCD_V_RES, fb);
}

/* ---- Draw a centered line of glyphs ------------------------------------- */

static void fb_draw_line(int page, int start_col, const int *glyphs, int count)
{
    int col = start_col;
    for (int i = 0; i < count; i++) {
        fb_draw_glyph(page, col, glyphs[i]);
        col += 8;
    }
}

/* ---- Display on or off -------------------------------------------------- */

void display_set_enabled(bool enabled)
{
    if (enabled) {
        ESP_LOGD(TAG, "Display enabled");
        esp_lcd_panel_disp_on_off(panel, true);
    } else {
        ESP_LOGD(TAG, "Display disabled");
        esp_lcd_panel_disp_on_off(panel, false);
    }
}

/* ---- Display rendering -------------------------------------------------- */

static void render_display(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t now = tv.tv_sec + gatt_svc_get_tz_quarter_hours() * 15 * 60;
    struct tm tm;
    gmtime_r(&now, &tm);

    fb_clear();

    switch (gatt_svc_display_mode) {
        case DISPLAY_MODE_BLANK:
            /* Don't draw anything, just clear the display */
            ESP_LOGD(TAG, "Display mode: BLANK");
            fb_flush();
            display_set_enabled(false);
            return;

        case DISPLAY_MODE_BUTTON: {
            int64_t uptime = esp_timer_get_time();
            if (uptime - button_time > 5 * 1000000) {
                ESP_LOGD(TAG,
                        "Display mode: BUTTON (last press %lld seconds ago)",
                        (uptime - button_time) / 1000000);
                display_set_enabled(false);
                fb_flush();
                return;
            }
            ESP_LOGI(TAG, "Display mode: BUTTON (last press %lld seconds ago)",
                        (uptime - button_time) / 1000000);
            break;
        }

        case DISPLAY_MODE_NORMAL:
            ESP_LOGD(TAG, "Display mode: NORMAL");
        default:
            /* Normal mode: always show sensor readings */
            break;
    }
    display_set_enabled(true);

    /* Page 3: HH:MM:SS */
    int clock[] = {
        tm.tm_hour / 10, tm.tm_hour % 10, GLYPH_COLON,
        tm.tm_min / 10,  tm.tm_min % 10,  GLYPH_COLON,
        tm.tm_sec / 10,  tm.tm_sec % 10,
    };
    fb_draw_line(3, 32, clock, 8);

    if (!sensors_valid) {
        fb_flush();
        return;
    }

    /* Page 4: XXXX.XXhPa — pressure in hPa */
    int press_hpa = (int)(gatt_svc_pressure / 100.0f);
    int press_dec = (int)(gatt_svc_pressure / 1.0f) % 100;
    if (press_dec < 0) press_dec = -press_dec;
    int pressure[] = {
        press_hpa / 1000 % 10, press_hpa / 100 % 10,
        press_hpa / 10 % 10,   press_hpa % 10,
        GLYPH_DOT,
        press_dec / 10, press_dec % 10,
        GLYPH_hP, GLYPH_a
    };
    fb_draw_line(4, 28, pressure, 9);

    /* Page 5: */
#ifdef DISPLAY_SHOW_FAHRENHEIT
    /* XXX°F — temperature converted from °C */
    float temp_f = gatt_svc_temperature * 9.0f / 5.0f + 32.0f;
    int tf = (int)temp_f;
    int temp[] = {
        tf / 100 % 10, tf / 10 % 10, tf % 10,
        GLYPH_DEG, GLYPH_F,
    };

    fb_draw_line(5, 28, temp, 5);
#else
    /* XX.X°C — temperature in °C with 0.1° resolution */
    int tc = (int)(gatt_svc_temperature * 10.0f);
    int temp[] = {
        tc / 100 % 10, tc / 10 % 10, GLYPH_DOT, tc % 10,
        GLYPH_DEG, GLYPH_C,
    };

    fb_draw_line(5, 28, temp, 6);
#endif

    /* Page 6: XX%RH — humidity */
    int hum = (int)gatt_svc_humidity;
    int humidity[] = {
        hum / 10 % 10, hum % 10, GLYPH_PCT, GLYPH_R, GLYPH_H
    };
    fb_draw_line(6, 28, humidity, 5);

    /* Page 7: Battery voltage in mV */
    int battery_mv = gatt_svc_battery_mv * 2; // Assuming a voltage divider
                                              // that halves the battery voltage
    int battery[] = {
        battery_mv / 1000 % 10, battery_mv / 100 % 10,
        battery_mv / 10 % 10, battery_mv % 10,
        GLYPH_m, GLYPH_V
    };
    fb_draw_line(7, 28, battery, 6);

    fb_flush();
}

/* ---- Display task ------------------------------------------------------- */

static void display_task(void *param)
{
    while (1) {
        render_display();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ---- Initialization ----------------------------------------------------- */

esp_err_t display_init(void)
{
    /* I2C master bus (new driver) */
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
    ESP_LOGI(TAG, "I2C initialized on SDA=%d, SCL=%d", I2C_SDA_GPIO, I2C_SCL_GPIO);

    /* LCD panel IO over I2C */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = LCD_I2C_ADDR,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    /* SSD1306 panel driver */
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));
    //ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    uint8_t contrast = 0xFF;
    esp_lcd_panel_io_tx_param(io_handle, 0x81, &contrast, 1);
    uint8_t precharge = 0xF1;
    esp_lcd_panel_io_tx_param(io_handle, 0xD9, &precharge, 1);
    uint8_t vcomh = 0x40;
    esp_lcd_panel_io_tx_param(io_handle, 0xDB, &vcomh, 1);
    ESP_LOGI(TAG, "SSD1306 initialized via esp_lcd");

    xTaskCreate(display_task, "display_task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    return ESP_OK;
}

i2c_master_bus_handle_t display_get_i2c_bus(void)
{
    return i2c_bus;
}
