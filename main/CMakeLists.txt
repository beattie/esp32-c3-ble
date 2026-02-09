idf_component_register(
    SRCS "display.c" "bmx280_task.c" "main.c" "gatt_svc.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver esp_lcd
)
