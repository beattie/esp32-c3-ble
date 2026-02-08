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

## Battery Power

### 4x AA Alkaline Pack (recommended)

A 4-cell AA holder wired in series to the board's `5V` and `GND` pins.

| Battery state | Cell voltage | Pack voltage | ADC pin voltage |
|---------------|-------------|-------------|-----------------|
| Fresh         | 1.5V        | 6.0V        | 3.0V            |
| Nominal       | 1.3V        | 5.2V        | 2.6V            |
| Low           | 1.1V        | 4.4V        | 2.2V            |
| Dead          | 1.0V        | 4.0V        | 2.0V            |

The onboard AMS1117-3.3 LDO needs ~1V headroom, so the effective cutoff is
~4.3V (1.075V/cell). Below this the 3.3V rail will droop and the ESP32-C3 may
brown out.

Estimated runtime: ~20–30 hours of active BLE on 2000mAh AAs (~60mA average
draw including BLE advertising + sensor reads).

### Battery voltage monitoring

A resistive voltage divider on **GPIO3 (ADC1_CH3)** scales the pack voltage
into the ESP32-C3 ADC range (0–3.3V).

**Circuit:**

```
VBAT (4.0–6.0V) ──┬── R1 100kΩ ──┬── R2 100kΩ ──┬── GND
                   │              │              │
                   │          GPIO3 (ADC)        │
                   │                             │
                  4x AA                         GND
```

**Wiring summary:**

| Connection          | Pin     |
|---------------------|---------|
| Battery + → R1 top  | —       |
| R1/R2 junction      | GPIO3   |
| R2 bottom            | GND     |
| Battery +           | 5V pin  |
| Battery −           | GND pin |

**Calculations:**

- Divider ratio: R2 / (R1 + R2) = 100k / 200k = 0.5
- VBAT = ADC_voltage × 2
- Quiescent current: 6V / 200kΩ = 30µA (negligible)
- ADC range: 2.0V (dead) to 3.0V (fresh) — fits within 0–3.3V

**ADC configuration (ESP-IDF):**

```c
#include "esp_adc/adc_oneshot.h"

// ADC1 channel 3 = GPIO3, 11dB attenuation for 0–3.1V range
adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
};
```

**Voltage-to-SOH mapping (approximate):**

| ADC voltage | Pack voltage | SOH   |
|------------|-------------|-------|
| 3.0V       | 6.0V        | 100%  |
| 2.6V       | 5.2V        | 75%   |
| 2.2V       | 4.4V        | 25%   |
| 2.0V       | 4.0V        | 0%    |

> **Note:** Alkaline discharge is non-linear. For better SOH estimates, use a
> lookup table calibrated against actual cell discharge curves rather than
> linear interpolation.

### Other battery options

| Setup | Voltage | Extra HW | Rechargeable | Notes |
|-------|---------|----------|--------------|-------|
| 4x AA → 5V pin | 6.0–4.0V | Battery holder only | No (unless NiMH) | Recommended |
| 2x AA + boost | 3.0V → 5V | Boost converter (~$1) | No (unless NiMH) | Compact, less efficient |
| Li-Po + TP4056 | 3.7V → 5V | TP4056 + boost (~$5) | Yes | Smallest, best for permanent install |

> **Tip:** Deep sleep gets the ESP32-C3 down to ~5µA between readings for
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
