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

#include <inttypes.h>

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx.h"
#include "stm32f0xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx.h"
#include "stm32f1xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32f3)
#include "stm32f3xx.h"
#include "stm32f3xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32g0)
#include "stm32g0xx.h"
#include "stm32g0xx_ll_gpio.h"
#else
#error "Add support for your STM32 family!"
#include <stophere>
#endif

typedef struct {
  GPIO_TypeDef *port;
  uint32_t pin;
} gpio_pin_t;

#include "autogen-pins-stm32.h"

static inline void stm32_gpio_configure(const gpio_pin_t gpio_pin,
                                        const uint32_t ll_mode,
                                        const uint32_t ll_pull) {
  LL_GPIO_InitTypeDef ll_gpio_init;
  ll_gpio_init.Pin = gpio_pin.pin;
  ll_gpio_init.Mode = ll_mode;
  ll_gpio_init.Pull = ll_pull;

  // These only have an effect if an output mode is selected
  ll_gpio_init.OutputType =
      LL_GPIO_OUTPUT_PUSHPULL;  // open-drain is possible too!
  ll_gpio_init.Speed = LL_GPIO_SPEED_FREQ_HIGH;

  LL_GPIO_Init(gpio_pin.port, &ll_gpio_init);
}

/* Configure a pin as an output. */
static inline void gpio_make_output(const gpio_pin_t gpio_pin) {
  // Configure pin for Push-pull. Open-drain is also available.
  stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_DOWN);
}

/*
 * Configure a pin as input, and enable its internal pullup resistor.
 */
static inline void gpio_make_input_enable_pullup(const gpio_pin_t gpio_pin) {
  stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
}

/*
 * Configure a pin as input, and enable its internal pulldown resistor.
 */
static inline void gpio_make_input_enable_pulldown(const gpio_pin_t gpio_pin) {
    stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_DOWN);
}

/*
 * Configure a pin as input and disable its pullup and pulldown resistors.
 */
#if defined(RULOS_ARM_stm32f1)
static inline void gpio_make_input_disable_pullup(const gpio_pin_t gpio_pin) {
  stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_FLOATING, LL_GPIO_PULL_UP);
}
#else
static inline void gpio_make_input_disable_pullup(const gpio_pin_t gpio_pin) {
  stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_NO);
}
#endif

static inline void gpio_make_adc_input(const gpio_pin_t gpio_pin) {
  stm32_gpio_configure(gpio_pin, LL_GPIO_MODE_ANALOG, LL_GPIO_PULL_NO);
}

/*
 * Assert an output pin HIGH.
 */
static inline void gpio_set(const gpio_pin_t gpio_pin) {
  LL_GPIO_SetOutputPin(gpio_pin.port, gpio_pin.pin);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(const gpio_pin_t gpio_pin) {
  LL_GPIO_ResetOutputPin(gpio_pin.port, gpio_pin.pin);
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_set_or_clr(const gpio_pin_t gpio_pin,
                                   const uint32_t state) {
  if (state) {
    gpio_set(gpio_pin);
  } else {
    gpio_clr(gpio_pin);
  }
}

/*
 * returns true if an input is being asserted LOW, false otherwise
 */
static inline int gpio_is_clr(const gpio_pin_t gpio_pin) {
  return (LL_GPIO_ReadInputPort(gpio_pin.port) & gpio_pin.pin) == 0;
}

/*
 * returns true if an input is being asserted HIGH, false otherwise
 */
static inline int gpio_is_set(const gpio_pin_t gpio_pin) {
  return !(gpio_is_clr(gpio_pin));
}
