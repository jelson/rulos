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

#include "chip.h"
#include "gpio_11xx_2.h"

#define GPIO0_00 (0), (0)
#define GPIO0_01 (0), (1)
#define GPIO0_02 (0), (2)
#define GPIO0_03 (0), (3)
#define GPIO0_04 (0), (4)
#define GPIO0_05 (0), (5)
#define GPIO0_06 (0), (6)
#define GPIO0_07 (0), (7)
#define GPIO0_08 (0), (8)
#define GPIO0_09 (0), (9)
#define GPIO0_10 (0), (10)
#define GPIO0_11 (0), (11)

#define GPIO1_00 (1), (0)
#define GPIO1_01 (1), (1)
#define GPIO1_02 (1), (2)
#define GPIO1_03 (1), (3)
#define GPIO1_04 (1), (4)
#define GPIO1_05 (1), (5)
#define GPIO1_06 (1), (6)
#define GPIO1_07 (1), (7)

#define GPIO2_00 (2), (0)
#define GPIO2_01 (2), (1)
#define GPIO2_02 (2), (2)
#define GPIO2_03 (2), (3)
#define GPIO2_04 (2), (4)
#define GPIO2_05 (2), (5)
#define GPIO2_06 (2), (6)
#define GPIO2_07 (2), (7)
#define GPIO2_08 (2), (8)
#define GPIO2_09 (2), (9)
#define GPIO2_10 (2), (10)
#define GPIO2_11 (2), (11)

#define GPIO3_00 (3), (0)
#define GPIO3_01 (3), (1)
#define GPIO3_02 (3), (2)
#define GPIO3_03 (3), (3)
#define GPIO3_04 (3), (4)
#define GPIO3_05 (3), (5)


static inline void gpio_make_input(const uint8_t port, const uint8_t pin) {
  Chip_GPIO_SetPinDIRInput(LPC_GPIO, port, pin);
}

static inline void gpio_make_output(const uint8_t port, const uint8_t pin) {
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, port, pin);
}

static inline void gpio_make_input_enable_pullup(const uint8_t port, const uint8_t pin) {
  gpio_make_input(port, pin);
}

static inline void gpio_make_input_enable_pulldown(const uint8_t port, const uint8_t pin) {
  gpio_make_input(port, pin);
}

static inline void gpio_make_input_disable_pullup(const uint8_t port, const uint8_t pin) {
  gpio_make_input(port, pin);
}


