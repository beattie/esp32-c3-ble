#include "oled.h"
#include "gatt_svc.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#define SSD1306_ADDR  0x3C
#define SSD1306_WIDTH 128

static const char *TAG = "oled";

/* ---- SSD1306 I2C helpers ------------------------------------------------ */

static esp_err_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    return i2c_master_write_to_device(I2C_NUM_0, SSD1306_ADDR,
                                      buf, 2, pdMS_TO_TICKS(50));
}

static esp_err_t oled_data(const uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    esp_err_t err = i2c_master_write_to_device(I2C_NUM_0, SSD1306_ADDR,
                                                buf, len + 1, pdMS_TO_TICKS(100));
    free(buf);
    return err;
}

/* ---- 8x8 font for digits and colon ------------------------------------- */

/* Column-major, LSB = top pixel, 8 columns per glyph */
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
};

#define GLYPH_COLON 10

/* Expand each bit to two bits vertically (for 2x scaling) */
static uint16_t expand_byte(uint8_t b)
{
    uint16_t r = 0;
    for (int i = 0; i < 8; i++) {
        if (b & (1 << i))
            r |= (3 << (2 * i));
    }
    return r;
}

/* ---- Display initialization --------------------------------------------- */

esp_err_t oled_init(void)
{
    static const uint8_t cmds[] = {
        0xAE,             /* Display OFF */
        0xD5, 0x80,       /* Clock div */
        0xA8, 0x3F,       /* Multiplex ratio (64-1) */
        0xD3, 0x00,       /* Display offset */
        0x40,             /* Start line 0 */
        0x8D, 0x14,       /* Charge pump enable */
        0x20, 0x00,       /* Horizontal addressing mode */
        0xA1,             /* Segment remap */
        0xC8,             /* COM scan decrement */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* Contrast */
        0xD9, 0xF1,       /* Precharge */
        0xDB, 0x40,       /* VCOMH deselect */
        0xA4,             /* Display from RAM */
        0xA6,             /* Normal (not inverted) */
        0xAF,             /* Display ON */
    };

    for (size_t i = 0; i < sizeof(cmds); i++) {
        esp_err_t err = oled_cmd(cmds[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Init cmd 0x%02X failed: %s", cmds[i], esp_err_to_name(err));
            return err;
        }
    }

    /* Clear entire display */
    uint8_t zeros[SSD1306_WIDTH];
    memset(zeros, 0, sizeof(zeros));
    for (int page = 0; page < 8; page++) {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        oled_data(zeros, SSD1306_WIDTH);
    }

    ESP_LOGI(TAG, "SSD1306 initialized");
    return ESP_OK;
}

/* ---- Display time (HH:MM:SS, 2x scaled, centered) ---------------------- */

void oled_show_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* Apply timezone offset */
    time_t now = tv.tv_sec + gatt_svc_get_tz_quarter_hours() * 15 * 60;
    struct tm tm;
    gmtime_r(&now, &tm);

    /* "HH:MM:SS" → 8 glyphs at 1x scale (8px each, 64px total, centered) */
    int glyphs[8] = {
        tm.tm_hour / 10, tm.tm_hour % 10,
        GLYPH_COLON,
        tm.tm_min / 10,  tm.tm_min % 10,
        GLYPH_COLON,
        tm.tm_sec / 10,  tm.tm_sec % 10,
    };

    uint8_t row[128];
    memset(row, 0, sizeof(row));

    /* 64px of glyphs, centered with 32px margin on each side */
    int col = 32;
    for (int g = 0; g < 8; g++) {
        const uint8_t *glyph = font[glyphs[g]];
        for (int x = 0; x < 8; x++) {
            row[col++] = glyph[x];
        }
    }

    /* Write to page 3 (vertically centered) */
    oled_cmd(0xB0 + 3);
    oled_cmd(0x00);
    oled_cmd(0x10);
    oled_data(row, 128);
}

/* ---- Turn off display -------------------------------------------------- */

void oled_off(void)
{
    oled_cmd(0xAE); /* Display OFF */
}

/* ---- Turn on display --------------------------------------------------- */

void oled_on(void)
{
    oled_cmd(0xAF); /* Display ON */
}