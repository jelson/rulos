/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#pragma once

#include <inttypes.h>

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx.h"
#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx.h"
#include "stm32f1xx_ll_gpio.h"
#else
#error "Add support for your STM32 family!"
#endif

#define GPIO_A0 (GPIOA), (LL_GPIO_PIN_0)
#define GPIO_A1 (GPIOA), (LL_GPIO_PIN_1)
#define GPIO_A2 (GPIOA), (LL_GPIO_PIN_2)
#define GPIO_A3 (GPIOA), (LL_GPIO_PIN_3)
#define GPIO_A4 (GPIOA), (LL_GPIO_PIN_4)
#define GPIO_A5 (GPIOA), (LL_GPIO_PIN_5)
#define GPIO_A6 (GPIOA), (LL_GPIO_PIN_6)
#define GPIO_A7 (GPIOA), (LL_GPIO_PIN_7)
#define GPIO_A8 (GPIOA), (LL_GPIO_PIN_8)
#define GPIO_A9 (GPIOA), (LL_GPIO_PIN_9)
#define GPIO_A10 (GPIOA), (LL_GPIO_PIN_10)
#define GPIO_A11 (GPIOA), (LL_GPIO_PIN_11)
#define GPIO_A12 (GPIOA), (LL_GPIO_PIN_12)
#define GPIO_A13 (GPIOA), (LL_GPIO_PIN_13)
#define GPIO_A14 (GPIOA), (LL_GPIO_PIN_14)
#define GPIO_A15 (GPIOA), (LL_GPIO_PIN_15)
#define GPIO_B0 (GPIOB), (LL_GPIO_PIN_0)
#define GPIO_B1 (GPIOB), (LL_GPIO_PIN_1)
#define GPIO_B2 (GPIOB), (LL_GPIO_PIN_2)
#define GPIO_B3 (GPIOB), (LL_GPIO_PIN_3)
#define GPIO_B4 (GPIOB), (LL_GPIO_PIN_4)
#define GPIO_B5 (GPIOB), (LL_GPIO_PIN_5)
#define GPIO_B6 (GPIOB), (LL_GPIO_PIN_6)
#define GPIO_B7 (GPIOB), (LL_GPIO_PIN_7)
#define GPIO_B8 (GPIOB), (LL_GPIO_PIN_8)
#define GPIO_B9 (GPIOB), (LL_GPIO_PIN_9)
#define GPIO_B10 (GPIOB), (LL_GPIO_PIN_10)
#define GPIO_B11 (GPIOB), (LL_GPIO_PIN_11)
#define GPIO_B12 (GPIOB), (LL_GPIO_PIN_12)
#define GPIO_B13 (GPIOB), (LL_GPIO_PIN_13)
#define GPIO_B14 (GPIOB), (LL_GPIO_PIN_14)
#define GPIO_B15 (GPIOB), (LL_GPIO_PIN_15)
#define GPIO_C0 (GPIOC), (LL_GPIO_PIN_0)
#define GPIO_C1 (GPIOC), (LL_GPIO_PIN_1)
#define GPIO_C2 (GPIOC), (LL_GPIO_PIN_2)
#define GPIO_C3 (GPIOC), (LL_GPIO_PIN_3)
#define GPIO_C4 (GPIOC), (LL_GPIO_PIN_4)
#define GPIO_C5 (GPIOC), (LL_GPIO_PIN_5)
#define GPIO_C6 (GPIOC), (LL_GPIO_PIN_6)
#define GPIO_C7 (GPIOC), (LL_GPIO_PIN_7)
#define GPIO_C8 (GPIOC), (LL_GPIO_PIN_8)
#define GPIO_C9 (GPIOC), (LL_GPIO_PIN_9)
#define GPIO_C10 (GPIOC), (LL_GPIO_PIN_10)
#define GPIO_C11 (GPIOC), (LL_GPIO_PIN_11)
#define GPIO_C12 (GPIOC), (LL_GPIO_PIN_12)
#define GPIO_C13 (GPIOC), (LL_GPIO_PIN_13)
#define GPIO_C14 (GPIOC), (LL_GPIO_PIN_14)
#define GPIO_C15 (GPIOC), (LL_GPIO_PIN_15)
#define GPIO_D0 (GPIOD), (LL_GPIO_PIN_0)
#define GPIO_D1 (GPIOD), (LL_GPIO_PIN_1)
#define GPIO_D2 (GPIOD), (LL_GPIO_PIN_2)
#define GPIO_D3 (GPIOD), (LL_GPIO_PIN_3)
#define GPIO_D4 (GPIOD), (LL_GPIO_PIN_4)
#define GPIO_D5 (GPIOD), (LL_GPIO_PIN_5)
#define GPIO_D6 (GPIOD), (LL_GPIO_PIN_6)
#define GPIO_D7 (GPIOD), (LL_GPIO_PIN_7)
#define GPIO_D8 (GPIOD), (LL_GPIO_PIN_8)
#define GPIO_D9 (GPIOD), (LL_GPIO_PIN_9)
#define GPIO_D10 (GPIOD), (LL_GPIO_PIN_10)
#define GPIO_D11 (GPIOD), (LL_GPIO_PIN_11)
#define GPIO_D12 (GPIOD), (LL_GPIO_PIN_12)
#define GPIO_D13 (GPIOD), (LL_GPIO_PIN_13)
#define GPIO_D14 (GPIOD), (LL_GPIO_PIN_14)
#define GPIO_D15 (GPIOD), (LL_GPIO_PIN_15)

// For when you simply must store the pin definition dynamically.
typedef struct {
  volatile uint8_t *port;
  uint32_t pin;
} IOPinDef;
#define PINDEF(IOPIN) \
  { IOPIN }
#define PINUSE(IOPIN) (IOPIN).port, (IOPIN).pin

static inline void stm32_gpio_configure(GPIO_TypeDef *port,
                                        const uint32_t ll_pin,
                                        const uint32_t ll_mode,
                                        const uint32_t ll_pull) {
  LL_GPIO_InitTypeDef ll_gpio_init;
  ll_gpio_init.Pin = ll_pin;
  ll_gpio_init.Mode = ll_mode;
  ll_gpio_init.Pull = ll_pull;

  // These only have an effect if an output mode is selected
  ll_gpio_init.OutputType =
      LL_GPIO_OUTPUT_PUSHPULL;  // open-drain is possible too!
  ll_gpio_init.Speed = LL_GPIO_SPEED_FREQ_HIGH;

  LL_GPIO_Init(port, &ll_gpio_init);
}

/* Configure a pin as an output. */
static inline void gpio_make_output(GPIO_TypeDef *port, const uint32_t pin) {
  // Configure pin for Push-pull. Open-drain is also available.
  stm32_gpio_configure(port, pin, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_DOWN);
}

/*
 * Configure a pin as input, and enable its internal pullup resistor.
 */
static inline void gpio_make_input_enable_pullup(GPIO_TypeDef *port,
                                                 const uint32_t pin) {
  stm32_gpio_configure(port, pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
}

/*
 * Configure a pin as input, and enable its internal pulldown resistor.
 */
static inline void gpio_make_input_enable_pulldown(GPIO_TypeDef *port,
                                                   const uint32_t pin) {
  stm32_gpio_configure(port, pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_DOWN);
}

/*
 * Configure a pin as input and disable its pullup and pulldown resistors.
 */
static inline void gpio_make_input_disable_pullup(GPIO_TypeDef *port,
                                                  const uint32_t pin) {
  stm32_gpio_configure(port, pin, LL_GPIO_MODE_FLOATING, LL_GPIO_PULL_UP);
}

/*
 * Assert an output pin HIGH.
 */
static inline void gpio_set(GPIO_TypeDef *port, const uint32_t pin) {
  LL_GPIO_SetOutputPin(port, pin);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(GPIO_TypeDef *port, const uint32_t pin) {
  LL_GPIO_ResetOutputPin(port, pin);
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_clr_set_or_clr(GPIO_TypeDef *port, const uint32_t pin,
                                       const uint32_t state) {
  if (state) {
    gpio_set(port, pin);
  } else {
    gpio_clr(port, pin);
  }
}

#if 0
/*
 * returns true if an input is being asserted LOW, false otherwise
 */
static inline int gpio_is_clr(volatile uint8_t *ddr, volatile uint8_t *port,
                              volatile uint8_t *pin, uint8_t bit) {
  return reg_is_clr(pin, bit);
}

/*
 * returns true if an input is being asserted HIGH, false otherwise
 */
static inline int gpio_is_set(volatile uint8_t *ddr, volatile uint8_t *port,
                              volatile uint8_t *pin, uint8_t bit) {
  return !(gpio_is_clr(ddr, port, pin, bit));
}

#endif
