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

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "core/hal.h"

#if defined(MCUatmega1284) || defined(MCUatmega1284p)
#define MCU1284_line
#elif defined(MCUatmega88) || defined(MCUatmega88p) || \
    defined(MCUatmega168) || defined(MCUatmega168p) || \
    defined(MCUatmega328) || defined(MCUatmega328p)
#define MCU328_line
#elif defined(MCUatmega8) || defined(MCUatmega16) || defined(MCUatmega32)
#define MCU8_line
#elif defined(MCUattiny24) || defined(MCUattiny44) || defined(MCUattiny84) || \
    defined(MCUattiny24a) || defined(MCUattiny44a) || defined(MCUattiny84a)
#define MCUtiny84_line
#elif defined(MCUattiny25) || defined(MCUattiny25v) || defined(MCUattiny45) || \
    defined(MCUattiny45v) || defined(MCUattiny85) || defined(MCUattiny85v)
#define MCUtiny85_line
#else
#error Unknown processor
#endif

typedef struct {
  volatile uint8_t *ddr;
  volatile uint8_t *port;
  volatile uint8_t *pin;
  uint8_t bit;
} gpio_pin_t;

#include "autogen-pins-avr.h"

/*
 * set a bit in a register
 */
static inline void reg_set(volatile uint8_t *reg, uint8_t bit) {
  *reg |= (1 << bit);
}

/*
 * clear a bit in a register
 */
static inline void reg_clr(volatile uint8_t *reg, uint8_t bit) {
  *reg &= ~(1 << bit);
}

static inline uint8_t reg_is_clr(volatile uint8_t *reg, uint8_t bit) {
  return ((*reg) & (1 << bit)) == 0;
}

static inline uint8_t reg_is_set(volatile uint8_t *reg, uint8_t bit) {
  return !reg_is_clr(reg, bit);
}

/* configure a pin as output */
static inline void gpio_make_output(const gpio_pin_t gpio_pin) {
  reg_set(gpio_pin.ddr, gpio_pin.bit);
}

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
 * assert an output pin HIGH
 */
static inline void gpio_set(const gpio_pin_t gpio_pin) {
  reg_set(gpio_pin.port, gpio_pin.bit);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(const gpio_pin_t gpio_pin) {
  reg_clr(gpio_pin.port, gpio_pin.bit);
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_set_or_clr(const gpio_pin_t gpio_pin,
                                   uint8_t onoff) {
  if (onoff)
    gpio_set(gpio_pin);
  else
    gpio_clr(gpio_pin);
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

#define _NOP()                   \
  do {                           \
    __asm__ __volatile__("nop"); \
  } while (0)

void sensor_interrupt_register_handler(Handler handler);

void avr_log(const char *fmt, ...);
void avr_assert(uint16_t line);
void hardware_assign_timer_handler(uint8_t timer_id, Handler handler);
void init_f_cpu(void);
extern uint32_t hardware_f_cpu;
