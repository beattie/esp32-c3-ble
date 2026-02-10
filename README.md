# ESP32-C3 BLE GATT Server

A minimal BLE GATT server for the AITRIP ESP32-C3 OLED board, built with ESP-IDF v5.5.2 and NimBLE.

The firmware advertises as **ESP32-C3-BLE** with a single custom service containing one read/write characteristic. Write bytes to the characteristic and read them back.

## Quick Start

### Option A: VSCode Dev Container

1. Open this repository in VSCode.
2. When prompted, **Reopen in Container** (requires Docker).
3. Build and flash:

   ```bash
   idf.py set-target esp32c3
   idf.py build
   idf.py flash monitor
   ```

### Option B: Command Line (devcontainer CLI)

1. Install the [devcontainer CLI](https://github.com/devcontainers/cli):

   ```bash
   npm install -g @devcontainers/cli
   ```

2. Start the dev container:

   ```bash
   devcontainer up --workspace-folder .
   ```

3. Build and flash:

   ```bash
   devcontainer exec --workspace-folder . bash -c \
     "source /opt/esp/idf/export.sh && idf.py build && idf.py -p /dev/ttyACM0 flash"
   ```

4. Scan with a BLE app (e.g. nRF Connect), connect to **ESP32-C3-BLE**, and read/write the characteristic.

## Debugging (JTAG)

The AITRIP ESP32-C3 board exposes a built-in USB-JTAG interface. Press **F5** in VSCode to start a debug session — OpenOCD connects via the built-in JTAG and GDB breaks at `app_main`.

## Custom Service UUIDs

| Item           | UUID                                   |
|----------------|----------------------------------------|
| Service        | `deadbeef-1000-2000-3000-aabbccddeeff` |
| Characteristic | `deadbeef-1001-2000-3000-aabbccddeeff` |

## Host Prerequisites (Linux)

Add a udev rule so the USB-JTAG device is accessible without root:

```bash
sudo tee /etc/udev/rules.d/60-esp32.rules <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="1001", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Notes on the AITRIP Board

- ESP32-C3 with 4 MB flash
- Built-in USB-CDC/JTAG on the USB-C port (appears as `/dev/ttyACM0`)
- 0.96" SSD1306 OLED (not used by this firmware — a good next step)
