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

#define USE_HAL_DRIVER
#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx.h"
#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx.h"
#else
#error "Add support for your STM32 family!"
#endif
#undef USE_HAL_DRIVER

#define GPIO_A0 (GPIOA), (GPIO_PIN_0)
#define GPIO_A1 (GPIOA), (GPIO_PIN_1)
#define GPIO_A2 (GPIOA), (GPIO_PIN_2)
#define GPIO_A3 (GPIOA), (GPIO_PIN_3)
#define GPIO_A4 (GPIOA), (GPIO_PIN_4)
#define GPIO_A5 (GPIOA), (GPIO_PIN_5)
#define GPIO_A6 (GPIOA), (GPIO_PIN_6)
#define GPIO_A7 (GPIOA), (GPIO_PIN_7)
#define GPIO_A8 (GPIOA), (GPIO_PIN_8)
#define GPIO_A9 (GPIOA), (GPIO_PIN_9)
#define GPIO_A10 (GPIOA), (GPIO_PIN_10)
#define GPIO_A11 (GPIOA), (GPIO_PIN_11)
#define GPIO_A12 (GPIOA), (GPIO_PIN_12)
#define GPIO_A13 (GPIOA), (GPIO_PIN_13)
#define GPIO_A14 (GPIOA), (GPIO_PIN_14)
#define GPIO_A15 (GPIOA), (GPIO_PIN_15)
#define GPIO_B0 (GPIOB), (GPIO_PIN_0)
#define GPIO_B1 (GPIOB), (GPIO_PIN_1)
#define GPIO_B2 (GPIOB), (GPIO_PIN_2)
#define GPIO_B3 (GPIOB), (GPIO_PIN_3)
#define GPIO_B4 (GPIOB), (GPIO_PIN_4)
#define GPIO_B5 (GPIOB), (GPIO_PIN_5)
#define GPIO_B6 (GPIOB), (GPIO_PIN_6)
#define GPIO_B7 (GPIOB), (GPIO_PIN_7)
#define GPIO_B8 (GPIOB), (GPIO_PIN_8)
#define GPIO_B9 (GPIOB), (GPIO_PIN_9)
#define GPIO_B10 (GPIOB), (GPIO_PIN_10)
#define GPIO_B11 (GPIOB), (GPIO_PIN_11)
#define GPIO_B12 (GPIOB), (GPIO_PIN_12)
#define GPIO_B13 (GPIOB), (GPIO_PIN_13)
#define GPIO_B14 (GPIOB), (GPIO_PIN_14)
#define GPIO_B15 (GPIOB), (GPIO_PIN_15)
#define GPIO_C0 (GPIOC), (GPIO_PIN_0)
#define GPIO_C1 (GPIOC), (GPIO_PIN_1)
#define GPIO_C2 (GPIOC), (GPIO_PIN_2)
#define GPIO_C3 (GPIOC), (GPIO_PIN_3)
#define GPIO_C4 (GPIOC), (GPIO_PIN_4)
#define GPIO_C5 (GPIOC), (GPIO_PIN_5)
#define GPIO_C6 (GPIOC), (GPIO_PIN_6)
#define GPIO_C7 (GPIOC), (GPIO_PIN_7)
#define GPIO_C8 (GPIOC), (GPIO_PIN_8)
#define GPIO_C9 (GPIOC), (GPIO_PIN_9)
#define GPIO_C10 (GPIOC), (GPIO_PIN_10)
#define GPIO_C11 (GPIOC), (GPIO_PIN_11)
#define GPIO_C12 (GPIOC), (GPIO_PIN_12)
#define GPIO_C13 (GPIOC), (GPIO_PIN_13)
#define GPIO_C14 (GPIOC), (GPIO_PIN_14)
#define GPIO_C15 (GPIOC), (GPIO_PIN_15)
#define GPIO_D0 (GPIOD), (GPIO_PIN_0)
#define GPIO_D1 (GPIOD), (GPIO_PIN_1)
#define GPIO_D2 (GPIOD), (GPIO_PIN_2)
#define GPIO_D3 (GPIOD), (GPIO_PIN_3)
#define GPIO_D4 (GPIOD), (GPIO_PIN_4)
#define GPIO_D5 (GPIOD), (GPIO_PIN_5)
#define GPIO_D6 (GPIOD), (GPIO_PIN_6)
#define GPIO_D7 (GPIOD), (GPIO_PIN_7)
#define GPIO_D8 (GPIOD), (GPIO_PIN_8)
#define GPIO_D9 (GPIOD), (GPIO_PIN_9)
#define GPIO_D10 (GPIOD), (GPIO_PIN_10)
#define GPIO_D11 (GPIOD), (GPIO_PIN_11)
#define GPIO_D12 (GPIOD), (GPIO_PIN_12)
#define GPIO_D13 (GPIOD), (GPIO_PIN_13)
#define GPIO_D14 (GPIOD), (GPIO_PIN_14)
#define GPIO_D15 (GPIOD), (GPIO_PIN_15)

// For when you simply must store the pin definition dynamically.
typedef struct {
  volatile uint8_t *port;
  uint16_t pin;
} IOPinDef;
#define PINDEF(IOPIN) \
  { IOPIN }
#define PINUSE(IOPIN) (IOPIN).port, (IOPIN).pin

static inline void stm32_gpio_configure(GPIO_TypeDef *port, const uint16_t pin,
                                        const uint32_t pull, uint32_t mode) {
  GPIO_InitTypeDef gpio_init;
  gpio_init.Pin = pin;
  gpio_init.Pull = pull;
  gpio_init.Mode = mode;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(port, &gpio_init);
}

/* Configure a pin as an output. */
static inline void gpio_make_output(GPIO_TypeDef *port, const uint16_t pin) {
  // Configure pin for Push-pull. Open-drain is also available.
  stm32_gpio_configure(port, pin, GPIO_NOPULL, GPIO_MODE_OUTPUT_PP);
}

/*
 * Configure a pin as input, and enable its internal pullup resistor.
 */
static inline void gpio_make_input_enable_pullup(GPIO_TypeDef *port,
                                                 const uint16_t pin) {
  stm32_gpio_configure(port, pin, GPIO_PULLUP, GPIO_MODE_INPUT);
}

/*
 * Configure a pin as input, and enable its internal pulldown resistor.
 */
static inline void gpio_make_input_enable_pulldown(GPIO_TypeDef *port,
                                                   const uint16_t pin) {
  stm32_gpio_configure(port, pin, GPIO_PULLDOWN, GPIO_MODE_INPUT);
}

/*
 * Configure a pin as input and disable its pullup and pulldown resistors.
 */
static inline void gpio_make_input_disable_pullup(GPIO_TypeDef *port,
                                                  const uint16_t pin) {
  stm32_gpio_configure(port, pin, GPIO_NOPULL, GPIO_MODE_INPUT);
}

/*
 * Assert an output pin HIGH.
 */
static inline void gpio_set(GPIO_TypeDef *port, const uint16_t pin) {
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(GPIO_TypeDef *port, const uint16_t pin) {
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_clr_set_or_clr(GPIO_TypeDef *port, const uint16_t pin,
                                       int state) {
  // Assumes GPIO_PIN_SET is 0, and GPIO_PIN_RESET is 1, which I
  // really hope is always true.
  HAL_GPIO_WritePin(port, pin, state);
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
