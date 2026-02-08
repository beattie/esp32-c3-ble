#include <oled.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "clock";

void clock_task(void *param)
{
    while (1) {
        oled_show_time();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- Initialization -----------------------------------------------------  */

esp_err_t clock_init(void)
{
    xTaskCreate(clock_task, "clock_task", 2048, NULL, tskIDLE_PRIORITY + 1,
         NULL);
    
    esp_err_t err = oled_init();
    return err;
}
