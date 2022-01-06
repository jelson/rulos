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

#include <stdbool.h>
#include <stdint.h>

#include "autogen-pins-esp32.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"

#define _NOP()                   \
  do {                           \
    __asm__ __volatile__("nop"); \
  } while (0)

uint32_t getApbFrequency();

//////////////// GPIO /////////////////////////////////

typedef uint32_t gpio_pin_t;

/* configure a pin as output */
static inline void gpio_make_output(const gpio_pin_t pin) {
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
static inline void gpio_set(const gpio_pin_t pin) {
  if (pin < 32) {
    GPIO.out_w1ts = ((uint32_t)1 << pin);
  } else if (pin < 34) {
    GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
  }
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(const gpio_pin_t pin) {
  if (pin < 32) {
    GPIO.out_w1tc = ((uint32_t)1 << pin);
  } else if (pin < 34) {
    GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
  }
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_set_or_clr(const gpio_pin_t pin, uint8_t onoff) {
  if (onoff) {
    gpio_set(pin);
  } else {
    gpio_clr(pin);
  }
}

/*
 * configure a pin as input, without touching the pullup config register
 *
 * Please do not call this function directly. It's ambiguous.
 */
static inline void _gpio_make_input(const gpio_pin_t pin) {
  if (pin < 32) {
    GPIO.enable_w1tc = ((uint32_t)1 << pin);
  } else {
    GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
  }
}

static inline void _gpio_enable_pull(const gpio_pin_t pin, const bool up) {
  uint32_t pinFunction;
  if (up) {
    pinFunction = FUN_PU;
  } else {
    pinFunction = FUN_PD;
  }

  // set drive strength
  pinFunction |= ((uint32_t)2 << FUN_DRV_S);

  // input enable but required for output as well?
  pinFunction |= FUN_IE;
  pinFunction |= ((uint32_t)2 << MCU_SEL_S);

  GPIO.pin[pin].val = pinFunction;
}

/*
 * configure a pin as input, and enable its internal pullup resistors
 */
static inline void gpio_make_input_enable_pullup(const gpio_pin_t pin) {
  _gpio_make_input(pin);
  _gpio_enable_pull(pin, true);
}


/*
 * configure a pin as input, and enable its internal pulldown resistors
 */
static inline void gpio_make_input_enable_pulldown(const gpio_pin_t pin) {
  _gpio_make_input(pin);
  _gpio_enable_pull(pin, false);
}

/*
 * configure a pin as input and disable its pullup resistor.
 */
static inline void gpio_make_input_disable_pullup(const gpio_pin_t pin) {
  _gpio_make_input(pin);
  _gpio_enable_pull(pin, false);
}

#if 0

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
