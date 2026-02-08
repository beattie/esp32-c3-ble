idf_component_register(
    SRCS "clock.c" "oled.c" "bmx280_task.c" "main.c" "gatt_svc.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver
)
