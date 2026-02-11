# Low Power / Deep Sleep Implementation Plan

## Context

The device runs at ~60-80mA continuously (BLE advertising + OLED + sensors), draining 4x AA batteries in ~2 days. Adding automatic light sleep and a deep sleep "power off" mode will dramatically improve battery life.

## Two Features

### 1. Automatic Light Sleep (always active)

- ESP-IDF PM framework + FreeRTOS tickless idle + BLE modem sleep
- Device automatically sleeps between BLE events, sensor reads, and display updates
- Button and BLE remain responsive — no user-visible change in behavior
- Expected idle current: ~1-3mA (vs ~60-80mA today)

### 2. Deep Sleep "Power Off" (user-triggered)

- Triggered by 3-second long button press or BLE command
- Device enters deep sleep (~5µA), OLED off, all peripherals reset
- Two wakeup variants via `#ifdef DEEP_SLEEP_WAKEUP_GPIO`:
  - **Defined** (e.g. GPIO3): button on RTC GPIO wakes device
  - **Not defined** (GPIO9 only): timer wakeup every 5min, advertise 10s, sleep again

## Files to Create

- `main/power.c` / `main/power.h` — light sleep init, deep sleep entry, wakeup reason check, deferred deep sleep (for BLE command)

## Files to Modify

### `sdkconfig.defaults` — Enable PM and BLE modem sleep

```
CONFIG_PM_ENABLE=y
CONFIG_PM_DFS_INIT_AUTO=y
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
CONFIG_BT_CTRL_MODEM_SLEEP=y
CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y
CONFIG_BT_CTRL_SLEEP_MODE_EFF=1
CONFIG_BT_CTRL_SLEEP_CLOCK_EFF=1
CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y
```

Then delete `sdkconfig` and run `idf.py reconfigure`.

### `main/button.c` / `main/button.h` — Add long press detection

- Add `button_check_long_press()` — polls GPIO9 level, returns true if held >= 3 seconds
- Called from display_task at 1Hz (no new task needed)

### `main/display.h` — Expose `display_set_enabled(bool)`

- Already implemented in display.c:108 but not declared in header

### `main/display.c` — Check long press in display_task loop

- Add `#include "power.h"`, call `button_check_long_press()` -> `power_enter_deep_sleep()`

### `main/gatt_svc.c` — Add power control characteristic

- UUID `deadbeef-1009-2000-3000-aabbccddeeff` (write-only, uint8)
- Writing `0x01` triggers deferred deep sleep (500ms delay so BLE response sends)

### `main/main.c` — Integration

- Call `power_check_wakeup_reason()` early (logs wakeup cause)
- Call `power_init()` after `nimble_port_freertos_init()` (enables light sleep)
- Timer-wakeup variant: if woke from timer, advertise 10s then re-enter deep sleep

### `main/CMakeLists.txt`

- Add `power.c` to SRCS, `esp_pm` to REQUIRES
- Commented-out `target_compile_definitions` for `DEEP_SLEEP_WAKEUP_GPIO=3`

### `tools/ble_test.py` — Add `--deep-sleep` flag

## Key Design Decisions

- No I2C deinit before deep sleep (hardware resets everything; app_main reinits on wake)
- No PM locks needed for I2C/ADC (new I2C master driver handles this internally)
- `power_init()` called after NimBLE starts (NimBLE manages its own PM locks)
- Long press polling in display_task avoids new task/timer complexity
- BLE deep sleep command uses esp_timer 500ms delay so BLE response can be sent
- System time is lost on deep sleep (clock resets to epoch until BLE client sets it again)

## Deep Sleep Wakeup Variants

### Variant A: `DEEP_SLEEP_WAKEUP_GPIO` defined (e.g., GPIO3)

- ESP32-C3 deep sleep GPIO wakeup only works on GPIO0-5 (RTC GPIOs)
- Button press on that RTC GPIO wakes device fully
- Device restarts via `app_main()` with normal init sequence

### Variant B: `DEEP_SLEEP_WAKEUP_GPIO` not defined

- Deep sleep with timer wakeup (default 5 minutes)
- Wakes briefly, advertises for 10 seconds, then sleeps again
- A connected BLE client can cancel the sleep cycle
- Used when the only button is on GPIO9 (not an RTC GPIO)

## Power Budget (4x AA, ~3000mAh)

| State | Current | Battery Life |
|-------|---------|-------------|
| Active continuous | ~60-80mA | ~2 days |
| Light sleep + BLE | ~1-3mA | ~40-120 days |
| Deep sleep | ~5uA | ~years |
| Deep sleep + timer burst (avg) | ~1-2mA | ~60-120 days |

## Verification

1. Build and flash with new sdkconfig
2. Check serial log for "Automatic light sleep enabled" message
3. Test button: short press -> display on 5s, long press (3s) -> "entering deep sleep" log -> device powers down
4. Test BLE: `python tools/ble_test.py --deep-sleep` -> device sleeps
5. Measure current with multimeter to verify light sleep savings
