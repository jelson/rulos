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
#include "core/hal.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_periph.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"

#define _NOP()                   \
  do {                           \
    __asm__ __volatile__("nop"); \
  } while (0)

uint32_t getApbFrequency();
static const int RULOS_ESP32_CORE_ID = 1;

//////////////// GPIO /////////////////////////////////

typedef uint32_t gpio_pin_t;

/* configure a pin as output */
static inline void gpio_make_output(const gpio_pin_t pin) {
  if (pin > 33) {
    // do nothing -- pins above 33 can only be inputs
    return;
  }

  // Switch the IO_MUX for this pin to plain GPIO function. Without
  // this, pins that boot with a peripheral function (e.g. pins 12-15
  // are JTAG at reset) ignore the output enable bit because the mux
  // is still routing the pin to the peripheral, not the GPIO matrix.
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);

  if (pin < 32) {
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
 * configure a pin as input with the given pull resistors. IDF's driver does
 * the per-chip routing: on the original ESP32 the RTC-capable pads' pulls are
 * in RTC_IO rather than IO_MUX, and pins 34-39 have none at all (asking for
 * one aborts).
 */
static inline void _gpio_make_input(const gpio_pin_t pin, const gpio_pull_mode_t pull) {
  ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT));
  ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)pin, pull));
}

/*
 * configure a pin as input, and enable its internal pullup resistors
 */
static inline void gpio_make_input_enable_pullup(const gpio_pin_t pin) {
  _gpio_make_input(pin, GPIO_PULLUP_ONLY);
}

/*
 * configure a pin as input, and enable its internal pulldown resistors
 */
static inline void gpio_make_input_enable_pulldown(const gpio_pin_t pin) {
  _gpio_make_input(pin, GPIO_PULLDOWN_ONLY);
}

/*
 * configure a pin as a floating input, with no pull resistors
 */
static inline void gpio_make_input_disable_pullup(const gpio_pin_t pin) {
  _gpio_make_input(pin, GPIO_FLOATING);
}

/*
 * returns true if an input is being asserted LOW, false otherwise
 */
static inline int gpio_is_clr(const gpio_pin_t pin) {
  return gpio_ll_get_level(&GPIO, (gpio_num_t)pin) == 0;
}

/*
 * returns true if an input is being asserted HIGH, false otherwise
 */
static inline int gpio_is_set(const gpio_pin_t gpio_pin) {
  return !(gpio_is_clr(gpio_pin));
}
