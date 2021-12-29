/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#include "autogen-pins-esp32.h"
#include "soc/gpio_struct.h"


#define _NOP()                   \
  do {                           \
    __asm__ __volatile__("nop"); \
  } while (0)

/* configure a pin as output */
static inline void gpio_make_output(const uint32_t pin) {
  if (pin > 33) {
    // do nothing -- pins above 33 can only be inputs
  } else if (pin < 32) {
    GPIO.enable_w1ts = ((uint32_t)1 << pin);
  } else {
    GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
  }
}

/*
 * assert an output pin HIGH
 */
static inline void gpio_set(const uint32_t pin) {
  if(pin < 32) {
    GPIO.out_w1ts = ((uint32_t)1 << pin);
  } else if(pin < 34) {
    GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
  }
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(const uint32_t pin) {
  if (pin < 32) {
    GPIO.out_w1tc = ((uint32_t)1 << pin);
  } else if (pin < 34) {
    GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
  }
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_set_or_clr(const uint32_t pin, uint8_t onoff) {
  if (onoff) {
    gpio_set(pin);
  } else {
    gpio_clr(pin);
  }
}

#if 0

/*
 * configure a pin as input, without touching the pullup config register
 *
 * Please do not call this function directly. It's ambiguous.
 */
static inline void _gpio_make_input(const gpio_pin_t gpio_pin) {
  reg_clr(gpio_pin.ddr, gpio_pin.bit);  // set direction as input
}

/*
 * configure a pin as input, and enable its internal pullup resistors
 */
static inline void gpio_make_input_enable_pullup(const gpio_pin_t gpio_pin) {
  _gpio_make_input(gpio_pin);
  reg_set(gpio_pin.port, gpio_pin.bit);  // enable internal pull-up resistors
}

/*
 * configure a pin as input and disable its pullup resistor.
 */
static inline void gpio_make_input_disable_pullup(const gpio_pin_t gpio_pin) {
  _gpio_make_input(gpio_pin);
  reg_clr(gpio_pin.port, gpio_pin.bit);  // disable internal pull-up resistor
}

/*
 * returns true if an input is being asserted LOW, false otherwise
 */
static inline int gpio_is_clr(const gpio_pin_t gpio_pin) {
  return reg_is_clr(gpio_pin.pin, gpio_pin.bit);
}

/*
 * returns true if an input is being asserted HIGH, false otherwise
 */
static inline int gpio_is_set(const gpio_pin_t gpio_pin) {
  return !(gpio_is_clr(gpio_pin));
}

#endif
