#include "button.h"
#include "battery.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BUTTON_GPIO 4
#define LED_GPIO 8

/* ---- Time Stamp for Button Press ----------------------------------------- */

// Time of last button press in microseconds
// Initialized to 10 seconds ago to avoid spurious display on startup
volatile int64_t button_time = -10000000;

/* ---- Button polling function --------------------------------------------- */
static bool button_was_pressed = false;

void button_poll(void)
{
    bool pressed = (button_read_mv() < 1500);
    if (pressed && !button_was_pressed) {
        int64_t now = esp_timer_get_time();
        if (now - button_time > 300000) {
            static uint8_t led_state = 0;
            led_state = !led_state;
            gpio_hold_dis(LED_GPIO);
            gpio_set_level(LED_GPIO, led_state);
            gpio_hold_en(LED_GPIO);
            button_time = now;
        }
    }
    button_was_pressed = pressed;
}

/* ---- Button initialization ------------------------------------------------ */

void button_init(void)
{
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    gpio_hold_en(LED_GPIO);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
}