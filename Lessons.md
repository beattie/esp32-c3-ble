# ESP32-C3 BLE Project Memory

## Project Overview
- ESP-IDF v5.5.2 project targeting AITRIP ESP32-C3 OLED board
- NimBLE BLE GATT server with custom read/write characteristic
- Devcontainer-based development with JTAG debugging

## Board Details
- AITRIP ESP32-C3 with 4MB flash, 0.96" SSD1306 OLED
- Built-in USB-CDC/JTAG on USB-C port → `/dev/ttyACM0`
- OpenOCD config: `board/esp32c3-builtin.cfg`
- Hardware watchpoint limit: 2

## Key Architecture
- `main/main.c` — app_main, NVS init, NimBLE host setup, GAP event handler, advertising
- `main/gatt_svc.c` — Custom GATT service with 128-bit UUIDs, read/write characteristic (64-byte buffer)
- `main/gatt_svc.h` — Public API: `gatt_svc_init()`, `gatt_svc_chr_val_handle`

## Custom UUIDs (little-endian in code)
- Service: `deadbeef-1000-2000-3000-aabbccddeeff`
- Characteristic: `deadbeef-1001-2000-3000-aabbccddeeff`

## NimBLE Patterns
- `nimble_port_init()` → configure `ble_hs_cfg` callbacks → `gatt_svc_init()` → `nimble_port_freertos_init()`
- GATT init order: `ble_svc_gap_init()` → `ble_svc_gatt_init()` → `ble_gatts_count_cfg()` → `ble_gatts_add_svcs()`
- Use `os_mbuf_append()` for reads, `ble_hs_mbuf_to_flat()` for writes
- Advertise on sync callback, re-advertise on disconnect

## JTAG Debugging (lessons learned)
- Use `cppdbg` type (not `gdbtarget` — ESP-IDF extension intercepts it)
- GDB path in container: `/opt/esp/tools/riscv32-esp-elf-gdb/16.3_20250913/riscv32-esp-elf-gdb/bin/riscv32-esp-elf-gdb`
- Start OpenOCD via GDB `shell` command in `setupCommands` (preLaunchTask background matchers are unreliable)
- Must use `postRemoteConnectCommands` for `mon reset halt` (needs active connection)
- Disable FreeRTOS RTOS awareness: `openocd -c "set ESP_RTOS none"` (cppdbg can't handle thread enumeration)
- Disable `remote.autoForwardPorts` — VSCode sends HTTP to forwarded GDB port, corrupting protocol
- Don't TCP-test port 3333 — sending data corrupts OpenOCD's GDB state; check log file instead
- `CONFIG_BTDM_CTRL_MODE_BLE_ONLY` doesn't exist on ESP32-C3 (BLE-only by hardware)
- OpenOCD writes to stderr — redirect with `2>&1` for log capture
- ESP-IDF container lacks `ss` command — use log file or `/dev/tcp` for port checks

## sdkconfig Keys
- `CONFIG_BT_NIMBLE_ENABLED=y` (not Bluedroid)
- `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` for JTAG stepping
