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

#undef FALSE
#undef TRUE

#include "chip/arm/lpc_chip_11cxx_lib/chip.h"
#include "chip/arm/lpc_chip_11cxx_lib/gpio_11xx_2.h"

/*
 * This silliness is required because the LPC11[UEA]XX do IO Control
 * differently than the other chips in the line. The chip libraries that NXP
 * provides have two different versions of Chip_IOCON_PinMuxSet. The U/E/A
 * libraries take a port number and pin number; other architectures pass in
 * an IOCON_ enum that combines port and pin number.
 *
 * However, all architectures take a port/pin number for other GPIO setting
 * commands, not the magical enum.
 *
 * We abstract the difference away by passing all three to our GPIO functions:
 * a port number, a pin number, and either the IOCON enum (for architectures
 * that define them) or an ignored 0 for architctures that don't.
 */
#if defined(CHIP_LPC11UXX) || defined(CHIP_LPC11EXX) || defined(CHIP_LPC11AXX)
#define RULOS_IOCON(x) (0)
typedef uint8_t CHIP_IOCON_PIO_T;
#else
#define RULOS_IOCON(x) (x)
#endif

#define GPIO0_00 (0), (0), RULOS_IOCON(IOCON_PIO0_0)
#define GPIO0_01 (0), (1), RULOS_IOCON(IOCON_PIO0_1)
#define GPIO0_02 (0), (2), RULOS_IOCON(IOCON_PIO0_2)
#define GPIO0_03 (0), (3), RULOS_IOCON(IOCON_PIO0_3)
#define GPIO0_04 (0), (4), RULOS_IOCON(IOCON_PIO0_4)
#define GPIO0_05 (0), (5), RULOS_IOCON(IOCON_PIO0_5)
#define GPIO0_06 (0), (6), RULOS_IOCON(IOCON_PIO0_6)
#define GPIO0_07 (0), (7), RULOS_IOCON(IOCON_PIO0_7)
#define GPIO0_08 (0), (8), RULOS_IOCON(IOCON_PIO0_8)
#define GPIO0_09 (0), (9), RULOS_IOCON(IOCON_PIO0_9)
#define GPIO0_10 (0), (10), RULOS_IOCON(IOCON_PIO0_10)
#define GPIO0_11 (0), (11), RULOS_IOCON(IOCON_PIO0_11)

#define GPIO1_00 (1), (0), RULOS_IOCON(IOCON_PIO1_0)
#define GPIO1_01 (1), (1), RULOS_IOCON(IOCON_PIO1_1)
#define GPIO1_02 (1), (2), RULOS_IOCON(IOCON_PIO1_2)
#define GPIO1_03 (1), (3), RULOS_IOCON(IOCON_PIO1_3)
#define GPIO1_04 (1), (4), RULOS_IOCON(IOCON_PIO1_4)
#define GPIO1_05 (1), (5), RULOS_IOCON(IOCON_PIO1_5)
#define GPIO1_06 (1), (6), RULOS_IOCON(IOCON_PIO1_6)
#define GPIO1_07 (1), (7), RULOS_IOCON(IOCON_PIO1_7)
#define GPIO1_08 (1), (8), RULOS_IOCON(IOCON_PIO1_8)
#define GPIO1_09 (1), (9), RULOS_IOCON(IOCON_PIO1_9)
#define GPIO1_10 (1), (10), RULOS_IOCON(IOCON_PIO1_10)
#define GPIO1_11 (1), (11), RULOS_IOCON(IOCON_PIO1_11)

#define GPIO2_00 (2), (0), RULOS_IOCON(IOCON_PIO2_0)
#define GPIO2_01 (2), (1), RULOS_IOCON(IOCON_PIO2_1)
#define GPIO2_02 (2), (2), RULOS_IOCON(IOCON_PIO2_2)
#define GPIO2_03 (2), (3), RULOS_IOCON(IOCON_PIO2_3)
#define GPIO2_04 (2), (4), RULOS_IOCON(IOCON_PIO2_4)
#define GPIO2_05 (2), (5), RULOS_IOCON(IOCON_PIO2_5)
#define GPIO2_06 (2), (6), RULOS_IOCON(IOCON_PIO2_6)
#define GPIO2_07 (2), (7), RULOS_IOCON(IOCON_PIO2_7)
#define GPIO2_08 (2), (8), RULOS_IOCON(IOCON_PIO2_8)
#define GPIO2_09 (2), (9), RULOS_IOCON(IOCON_PIO2_9)
#define GPIO2_10 (2), (10), RULOS_IOCON(IOCON_PIO2_10)
#define GPIO2_11 (2), (11), RULOS_IOCON(IOCON_PIO2_11)

#define GPIO3_00 (3), (0), RULOS_IOCON(IOCON_PIO3_0)
#define GPIO3_01 (3), (1), RULOS_IOCON(IOCON_PIO3_1)
#define GPIO3_02 (3), (2), RULOS_IOCON(IOCON_PIO3_2)
#define GPIO3_03 (3), (3), RULOS_IOCON(IOCON_PIO3_3)
#define GPIO3_04 (3), (4), RULOS_IOCON(IOCON_PIO3_4)
#define GPIO3_05 (3), (5), RULOS_IOCON(IOCON_PIO3_5)

/*
 * IOCON abstracted away to work on either the platforms that take a port/pin
 * and those that take an IOCON constant.
 */
static inline void gpio_iocon(const uint8_t port, const uint8_t pin,
                              const CHIP_IOCON_PIO_T iocon, uint32_t modefunc) {
#if defined(CHIP_LPC11UXX) || defined(CHIP_LPC11EXX) || defined(CHIP_LPC11AXX)
  Chip_IOCON_PinMuxSet(LPC_IOCON, port, pin, modefunc);
#else
  Chip_IOCON_PinMuxSet(LPC_IOCON, iocon, modefunc);
#endif
}

/*
 * configure a pin as output
 */
static inline void gpio_make_output(const uint8_t port, const uint8_t pin,
                                    const CHIP_IOCON_PIO_T iocon) {
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, port, pin);
}

/*
 * configure a pin as input, without touching the pullup config register
 */
static inline void gpio_make_input(const uint8_t port, const uint8_t pin,
                                   const CHIP_IOCON_PIO_T iocon) {
  Chip_GPIO_SetPinDIRInput(LPC_GPIO, port, pin);
}

/*
 * configure a pin as input, and enable its internal pullUP resistor
 */
static inline void gpio_make_input_enable_pullup(const uint8_t port,
                                                 const uint8_t pin,
                                                 const CHIP_IOCON_PIO_T iocon) {
  gpio_make_input(port, pin, iocon);
  gpio_iocon(port, pin, iocon, IOCON_FUNC0 | IOCON_MODE_PULLUP);
}

/*
 * configure a pin as input, and enable its internal pullDOWN resistor
 */
static inline void gpio_make_input_enable_pulldown(
    const uint8_t port, const uint8_t pin, const CHIP_IOCON_PIO_T iocon) {
  gpio_make_input(port, pin, iocon);
  gpio_iocon(port, pin, iocon, IOCON_FUNC0 | IOCON_MODE_PULLDOWN);
}

/*
 * configure a pin as input and disable its pullup and pulldown resistors.
 */
static inline void gpio_make_input_disable_pullup(
    const uint8_t port, const uint8_t pin, const CHIP_IOCON_PIO_T iocon) {
  gpio_make_input(port, pin, iocon);
  gpio_iocon(port, pin, iocon, IOCON_FUNC0 | IOCON_MODE_INACT);
}

/*
 * assert an output pin HIGH
 */
static inline void gpio_set(const uint8_t port, const uint8_t pin,
                            const CHIP_IOCON_PIO_T iocon) {
  Chip_GPIO_SetPinOutHigh(LPC_GPIO, port, pin);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(const uint8_t port, const uint8_t pin,
                            const CHIP_IOCON_PIO_T iocon) {
  Chip_GPIO_SetPinOutLow(LPC_GPIO, port, pin);
}

static inline void gpio_set_or_clr(const uint8_t port, const uint8_t pin,
                                   const CHIP_IOCON_PIO_T iocon,
                                   uint8_t onoff) {
  if (onoff) {
    gpio_set(port, pin, iocon);
  } else {
    gpio_clr(port, pin, iocon);
  }
}

static inline int gpio_is_set(const uint8_t port, const uint8_t pin,
                              const CHIP_IOCON_PIO_T iocon) {
  return Chip_GPIO_GetPinState(LPC_GPIO, port, pin);
}

static inline int gpio_is_clr(const uint8_t port, const uint8_t pin,
                              const CHIP_IOCON_PIO_T iocon) {
  return !gpio_is_set(port, pin, iocon);
}
