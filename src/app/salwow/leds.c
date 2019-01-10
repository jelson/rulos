#include "leds.h"

#ifndef SIM

#include "hardware.h"

#define GREEN_LED_PIN GPIO_D7

void leds_init() {
  // init green led
  gpio_make_output(GREEN_LED_PIN);
}

void leds_green(r_bool state) { gpio_set_or_clr(GREEN_LED_PIN, state); }

#else

void leds_init() {}
void leds_green(r_bool state) {}

#endif
