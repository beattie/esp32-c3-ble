# ESP-IDF Reference Links

Quick reference for the ESP-IDF APIs and libraries used in this project.

## FreeRTOS Tasks

- [FreeRTOS (IDF) API — ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/freertos_idf.html) — `xTaskCreate`, `xTaskCreatePinnedToCore`, task management
- [FreeRTOS Overview](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html) — conceptual overview, startup tasks, SMP

On ESP32-C3 (single-core), `xTaskCreate` is sufficient — no need for `xTaskCreatePinnedToCore`.

## I2C Driver

- [I2C Driver — ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/i2c.html) — covers both the new `i2c_master.h` driver and the legacy `i2c.h` driver (deprecated, removed in IDF v6.0)
- [Migration Guide (legacy → new)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-5.x/5.2/peripherals.html) — v5.2 peripheral changes

The two drivers cannot coexist — include `i2c.h` for legacy or `i2c_master.h` for new.

## BMP280 / BME280 Libraries

### esp-idf-bmx280 (recommended)
- [GitHub — utkumaden/esp-idf-bmx280](https://github.com/utkumaden/esp-idf-bmx280)
- Supports BMP280 + BME280, both legacy and new I2C drivers
- Add via `idf_component.yml`:
  ```yaml
  dependencies:
    bmx280:
      git: https://github.com/utkumaden/esp-idf-bmx280.git
  ```

### Espressif official
- [espressif/bme280 — Component Registry](https://components.espressif.com/components/espressif/bme280) — `idf.py add-dependency "espressif/bme280^0.1.1"`

### Community (UncleRus)
- [esp-idf-lib/bmp280 — Component Registry](https://components.espressif.com/components/esp-idf-lib/bmp280)
- [esp-idf-lib BMP280 docs](https://esp-idf-lib.readthedocs.io/en/latest/groups/bmp280.html)

### Bosch official
- [BMP2 Sensor API (GitHub)](https://github.com/boschsensortec/BMP2_SensorAPI) — platform-independent C driver for BMP280
- [BME280 Sensor API (GitHub)](https://github.com/boschsensortec/BME280_SensorAPI)
- [BMP280 Datasheet (PDF)](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf)
- [BME280 Datasheet (PDF)](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)

## ESP-IDF Component Manager

- [Component Manager Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-component-manager.html) — how dependencies work, `managed_components/`, build integration
- [idf_component.yml Reference](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html) — manifest file format, version ranges, git sources

Add a dependency: `idf.py add-dependency "namespace/component^version"`
