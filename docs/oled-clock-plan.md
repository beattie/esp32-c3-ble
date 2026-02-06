# OLED Clock — Implementation Plan

## Goal
Display a 24-hour clock (HH:MM:SS) on the onboard SSD1306 OLED. Time and timezone are set/read via BLE GATT characteristics. Timezone persists to NVS.

## New files

### `main/idf_component.yml`
```yaml
dependencies:
  espressif/ssd1306: "^1.0.0"
```

### `main/oled_clock.h`
Public API + I2C GPIO pin defines (GPIO5/6, easy to change):
- `oled_clock_init()` — init I2C, SSD1306, load TZ from NVS, start display task
- `oled_clock_set_time(int64_t unix_seconds)` — calls `settimeofday()`
- `oled_clock_get_time()` → `int64_t` — calls `gettimeofday()`
- `oled_clock_set_tz_offset(int16_t minutes)` — saves to NVS
- `oled_clock_get_tz_offset()` → `int16_t`

### `main/oled_clock.c`
- I2C bus init (legacy driver, 400kHz)
- SSD1306 create/clear
- TZ load/save via NVS (namespace `"oled_clock"`, key `"tz_offset"`)
- `clock_display_task`: every 500ms, apply TZ offset to system time, render HH:MM:SS using `ssd1306_draw_3216char()` (32x16 font — 8 chars fills 128px width exactly, centered at y=16)
- Before time is set (epoch < year 2020): show "--:--:--" placeholder

## GATT characteristics (added to existing service in `gatt_svc.c`)

| Char | UUID | R/W | Format |
|------|------|-----|--------|
| Time | `deadbeef-1005-2000-3000-aabbccddeeff` | R/W | `int64_t` LE (UNIX seconds) |
| Timezone | `deadbeef-1006-2000-3000-aabbccddeeff` | R/W | `int16_t` LE (minutes from UTC) |

UUIDs 1002–1004 reserved for future BME280.

## Changes to existing files

### `main/gatt_svc.c`
- Add `#include "oled_clock.h"`
- Add UUID definitions for `chr_time_uuid` (1005) and `chr_tz_uuid` (1006)
- Add `time_access_cb` — read: `oled_clock_get_time()` → `os_mbuf_append`; write: validate 8 bytes → `oled_clock_set_time()`
- Add `tz_access_cb` — read: `oled_clock_get_tz_offset()` → `os_mbuf_append`; write: validate 2 bytes → `oled_clock_set_tz_offset()`
- Add both characteristics to `gatt_svr_svcs[]` before terminator

### `main/main.c`
- Add `#include "oled_clock.h"`
- Call `oled_clock_init()` after `nvs_flash_init()`, before `nimble_port_init()`

### `main/CMakeLists.txt`
```cmake
idf_component_register(
    SRCS "main.c" "gatt_svc.c" "oled_clock.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver
)
```

## Display layout
```
+---128px---+
|           |  16px top margin
| HH:MM:SS |  32px tall (3216 font, 8 chars x 16px = 128px wide)
|           |  16px bottom margin
+---128px---+
```

## Notes
- I2C GPIO pins default to 5/6 — user must verify against board silkscreen
- Legacy I2C driver (used by espressif/ssd1306) may emit deprecation warnings — cosmetic only
- `int16_t` TZ reads/writes are atomic on RISC-V, no mutex needed
- Time lost on power cycle (no battery-backed RTC) — must re-sync via BLE

## Verification
1. `idf.py build` — confirm clean compile
2. `idf.py flash monitor` — confirm OLED shows "--:--:--"
3. Write timestamp via BLE: `struct.pack('<q', int(time.time()))` → UUID 1005
4. Write TZ via BLE: `struct.pack('<h', 60)` → UUID 1006 (UTC+1)
5. Verify OLED displays correct local time in 24h format
6. Reboot — confirm TZ persists, time resets to placeholder
