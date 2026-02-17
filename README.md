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

### Option C: Command Line (avoid devcontainer CLI)

1. Find the docker container:

   ```bash
   docker ps -a
   ```

2. Start the docker container:

   ```bash
   docker start <container>
   ```

3. Build:

   ```bash
   docker exec <container> bash -c "source /opt/esp/idf/export.sh && cd /workspaces/esp32-c3-ble && idf.py build"
   ```

4. Build and flash:

    ```bash
    docker exec <container> bash -c "source /opt/esp/idf/export.sh && cd /workspaces/esp32-c3-ble && idf.py build && idf.py -p /dev/ttyACM0 flash"
    ```

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

## Light Sleep and GPIO

This project uses automatic light sleep for power saving (~18-21mA average vs ~60-80mA without). Light sleep introduces several GPIO issues on the ESP32-C3:

### GPIO Outputs Glitch During Sleep Transitions

GPIO output pins lose their state during light sleep entry/exit, causing visible glitching (e.g. LEDs flickering). The fix is `gpio_hold_en()` to latch the output state:

```c
gpio_hold_dis(pin);       // release latch
gpio_set_level(pin, val); // change state
gpio_hold_en(pin);        // re-latch
```

### Button Input via ADC

The button on GPIO4 is read using ADC (`adc_oneshot_read()` on ADC1_CHANNEL_4) rather than `gpio_get_level()`. During testing, digital GPIO reads appeared unreliable during light sleep, but this may have been a misdiagnosis caused by the GPIO output glitching described above. The LED was flickering due to `gpio_hold_en()` not being used, which made every button detection approach appear to fail. ADC reads (threshold: <1500mV = pressed, ~2888mV = open with internal pull-up) work reliably and were kept as the solution.

Digital GPIO reads with `gpio_hold_en()` on the LED output have not been re-tested and may work fine. GPIO interrupts (both edge and level triggered) are known to fire spuriously during sleep transitions regardless.

### USB-CDC/JTAG Incompatible with Light Sleep

The built-in USB-Serial/JTAG peripheral cannot respond to host USB polls during light sleep, causing disconnection and enumeration failures. Workaround: hold BOOT button while plugging in to enter download mode for flashing. A 5-second delay in `power_init()` provides a window for `idf.py monitor` to attach before light sleep activates.

## Notes on the AITRIP Board

- ESP32-C3 with 4 MB flash
- Built-in USB-CDC/JTAG on the USB-C port (appears as `/dev/ttyACM0`)
- 0.96" SSD1306 OLED
- Blue LED on GPIO8
- BOOT button on GPIO9
