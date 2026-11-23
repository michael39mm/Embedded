#include "pico/stdlib.h"

#define PIN_RELAY 13  /* GP13 = Physical Pin 17 */

static void magnet_on(void) {
    gpio_put(PIN_RELAY, 0);
    gpio_set_dir(PIN_RELAY, GPIO_OUT);
}

static void magnet_off(void) {
    gpio_set_dir(PIN_RELAY, GPIO_IN);
    gpio_disable_pulls(PIN_RELAY);
}

int main(void) {
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(PIN_RELAY);
    gpio_disable_pulls(PIN_RELAY);
    magnet_off();

    while (true) {
        gpio_put(LED_PIN, 1);
        magnet_on();
        sleep_ms(2000);

        gpio_put(LED_PIN, 0);
        magnet_off();
        sleep_ms(2000);
    }
}