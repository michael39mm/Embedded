#include "magnet.h"
#include <stdio.h>
#include "pico/stdlib.h"

void magnet_init(void) {
    /* Start in high-Z input mode (open-drain OFF) so the relay never clicks on boot */
    gpio_init(PIN_RELAY);
    gpio_disable_pulls(PIN_RELAY);
    gpio_put(PIN_RELAY, 0);
    gpio_set_dir(PIN_RELAY, GPIO_IN);
}

void magnet_on(void) {
    /* Open-drain active-LOW: pull GP13 to GND to energize the 5V relay */
    gpio_put(PIN_RELAY, 0);
    gpio_set_dir(PIN_RELAY, GPIO_OUT);
    printf("info string [MAGNET] ON\n");
}

void magnet_off(void) {
    /* Open-drain release: float GP13 as input so the 5V relay turns cleanly OFF */
    gpio_set_dir(PIN_RELAY, GPIO_IN);
    gpio_disable_pulls(PIN_RELAY);
    printf("info string [MAGNET] OFF\n");
}