idf_component_register(
    SRCS "battery.c" "display.c" "bmx280_sensor.c" "main.c" "gatt_svc.c" "sensor_task.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver esp_lcd esp_adc
)
