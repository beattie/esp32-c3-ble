#include "button.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"

#define BUTTON_GPIO 4

/* ---- Time Stamp for Button Press ----------------------------------------- */

// Time of last button press in microseconds
// Initialized to 10 seconds ago to avoid spurious display on startup
volatile int64_t button_time = -10000000;

/* ---- Button interrupt handler -------------------------------------------- */

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();
    
    // Debounce: ignore presses within 200ms
    if (now - button_time > 200000) {
        button_time = now;
    }
}   


/* ---- Button initialization ------------------------------------------------ */
void button_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_LOW_LEVEL, // Interrupt on low level (button press)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable internal pull-up resistor
    };
    
    gpio_config(&io_conf);
    
    gpio_sleep_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    // Install ISR service and add handler for the button GPIO
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void *)BUTTON_GPIO);
}