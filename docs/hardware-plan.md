# Hardware Plan

Integration plan for adding a BME280 temperature/humidity/pressure sensor to the
ESP32-C3 BLE project. The BME280 shares the I2C bus with the existing SSD1306
OLED display.

## Wiring

| BME280 Pin | ESP32-C3 GPIO | Notes |
|------------|---------------|-------|
| VCC        | 3V3           | BME280 breakouts typically have an onboard regulator |
| GND        | GND           | |
| SDA        | GPIO5 (?)     | Check board silkscreen — may be GPIO8 |
| SCL        | GPIO6 (?)     | Check board silkscreen — may be GPIO9 |

> **Before wiring:** Identify the I2C GPIOs on the AITRIP board. The SSD1306
> OLED is already connected to the I2C bus. Check the board silkscreen or trace
> which GPIOs the OLED uses — those are your SDA/SCL lines. Common candidates
> are GPIO5/GPIO6 or GPIO8/GPIO9.

**I2C addresses on the shared bus:**

| Device  | Address |
|---------|---------|
| SSD1306 | 0x3C   |
| BME280  | 0x76 (SDO→GND) or 0x77 (SDO→VCC) |

Both devices share the same SDA/SCL lines. No additional pull-ups should be
needed if the OLED breakout already has them.

## Library Options

| Library | Source | Pros | Cons |
|---------|--------|------|------|
| **espressif/bme280** | [Component Registry](https://components.espressif.com/) | Official, `idf.py add-dependency "espressif/bme280"` | Less community usage |
| **esp-idf-lib bmp280** | [UncleRus](https://github.com/UncleRus/esp-idf-lib) | Mature, well-documented, supports BMP280+BME280 | Pulls in the full esp-idf-lib as a submodule |
| **esp-idf-bmx280** | [utkumaden](https://github.com/utkumaden/esp-idf-bmx280) | Supports new `i2c_master.h` driver (ESP-IDF 5.3+) | Smaller community |
| **Manual (Bosch API)** | [Bosch BSEC](https://github.com/boschsensortec/BME280_SensorAPI) | Full control, no dependencies | Most code to write, must handle I2C yourself |

**Recommendation:** Start with **esp-idf-bmx280** — it supports the current
ESP-IDF 5.x I2C master driver and is easy to integrate as a component.

## Implementation Outline

### New files

- `main/bme280_sensor.h` — public API
- `main/bme280_sensor.c` — I2C init, sensor reading task, GATT value updates

### New BLE characteristics

Following the existing UUID pattern (`deadbeef-XXXX-2000-3000-aabbccddeeff`):

| Characteristic | UUID suffix | Properties | Format |
|----------------|-------------|------------|--------|
| Temperature    | `1002`      | Read, Notify | int16 (0.01 C units) |
| Humidity       | `1003`      | Read, Notify | uint16 (0.01 % units) |
| Pressure       | `1004`      | Read, Notify | uint32 (Pa) |

UUID examples:
```
deadbeef-1002-2000-3000-aabbccddeeff  (temperature)
deadbeef-1003-2000-3000-aabbccddeeff  (humidity)
deadbeef-1004-2000-3000-aabbccddeeff  (pressure)
```

### Sensor task

A FreeRTOS task polls the BME280 at a configurable interval (e.g. every 5
seconds) and updates the characteristic values. On change, it sends BLE
notifications to subscribed clients.

```
bme280_task(void *param)
    loop:
        read sensor → temp, humidity, pressure
        update GATT characteristic values
        if client subscribed → ble_gatts_notify()
        vTaskDelay(pdMS_TO_TICKS(5000))
```

### Changes to existing files

**`main/CMakeLists.txt`** — add new source and I2C dependency:
```cmake
idf_component_register(
    SRCS "main.c" "gatt_svc.c" "bme280_sensor.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver
)
```

**`main/gatt_svc.c`** — add three new characteristics to the existing service
definition, each with a read callback and BLE_GATT_CHR_F_NOTIFY flag.

**`main/main.c`** — call `bme280_sensor_init()` after `gatt_svc_init()` to
start the sensor task.

### GATT access callback pattern

The sensor characteristics are read-only from the BLE side. The access callback
follows the same pattern as the existing characteristic:

```c
case BLE_GATT_ACCESS_OP_READ_CHR:
    rc = os_mbuf_append(ctxt->om, &temperature, sizeof(temperature));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
```

## BLE Test Script Update

Extend `tools/ble_test.py` to read the sensor characteristics:

```python
TEMP_UUID     = "deadbeef-1002-2000-3000-aabbccddeeff"
HUMIDITY_UUID = "deadbeef-1003-2000-3000-aabbccddeeff"
PRESSURE_UUID = "deadbeef-1004-2000-3000-aabbccddeeff"

# Read sensor values
temp_raw = await client.read_gatt_char(TEMP_UUID)
temperature = int.from_bytes(temp_raw, "little", signed=True) / 100.0
print(f"Temperature: {temperature} C")

# Subscribe to notifications
def on_notify(handle, data):
    print(f"Notification [{handle}]: {data.hex()}")

await client.start_notify(TEMP_UUID, on_notify)
await asyncio.sleep(30)  # listen for 30s
await client.stop_notify(TEMP_UUID)
```

## Battery Power (Optional)

Three options for running untethered, away from USB power.

### Option A: 3x AA/AAA (simplest — no extra modules)

Wire a 3x AA holder in series straight to the board's `5V`/`VIN` and `GND`
pins. Voltage ranges from 4.5V (fresh) to ~3.0V (dead). The onboard 3.3V LDO
handles regulation. Estimated runtime ~15–20 hours of active BLE on 2000mAh AAs.

```
3x AA (4.5V) ──► VIN/GND
```

### Option B: 2x AA/AAA + boost converter

Use a boost converter module (MT3608 or TPS61200, ~$1) set to 3.3V output,
wired to the board's `3V3` and `GND` pins. Handles the 3.0V→1.8V range of 2x
cells. Less efficient than Option A — the converter wastes 10–15% as heat and
works harder as voltage drops.

```
2x AA (3.0V) ──► Boost converter (3.3V out) ──► 3V3/GND
```

### Option C: Li-Po + TP4056 charge module

A single-cell 3.7V Li-Po with a TP4056 charge board (~$1, get the version with
DW01A protection IC). Rechargeable via USB, smallest form factor.

```
USB-C ──► TP4056 B+/B- ──► Li-Po cell
                 OUT+/OUT- ──► [switch] ──► VIN/GND
```

### Comparison

| Setup | Extra HW | Cost | Rechargeable | Notes |
|-------|----------|------|--------------|-------|
| 3x AA → VIN | Battery holder only | ~$0.50 | No (unless NiMH) | Simplest |
| 2x AA + boost | Boost converter | ~$1.50 | No (unless NiMH) | Compact, less efficient |
| Li-Po + TP4056 | TP4056 module, Li-Po cell | ~$5 | Yes | Smallest, best for permanent install |

> **Tip:** Add a voltage divider on an ADC pin (GPIO0–GPIO4) to monitor battery
> level over BLE. Deep sleep gets the ESP32-C3 down to ~5uA between readings for
> long battery life.

## Checklist

- [ ] Identify I2C GPIOs on the AITRIP board
- [ ] Wire BME280 breakout (4 wires)
- [ ] Pick a library and add it as a component
- [ ] Implement `bme280_sensor.c/.h`
- [ ] Add GATT characteristics to `gatt_svc.c`
- [ ] Init sensor in `main.c`
- [ ] Update `tools/ble_test.py`
- [ ] Test with `idf.py build && idf.py flash monitor`
