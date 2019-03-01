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

#include "core/hal.h"
#include "core/hardware.h"
#include "core/logging.h"

#undef FALSE
#undef TRUE

#include "chip.h"

// This is the first entry point defined by the LPC startup file:
// meant for early setup such as clock configuration. This is done
// before, for example, bss is copied into RAM. It calls _start()
// next, which we define in arm/common/core/hardware.c.
void SystemInit() {
  // LPC library function that configures the internal RC oscillator as the
  // system clock
  Chip_SystemInit();
}

// RULOS-specific init function. We use this to initialize various modules on
// the chip.
void hal_init() {
  // init GPIO subsystem
  Chip_GPIO_Init(LPC_GPIO);

  // Eventually, more init will go here as we use more peripherals
}

#ifndef CRYSTAL
#define CRYSTAL 0
#endif

// A variable called OscRateIn is expected to be defined by the LPC
// libraries to define the rate of external oscillator or crystal, if
// there is one. This statement binds to the value provided from the
// RULOS build system.
const uint32_t OscRateIn = CRYSTAL;

static void* g_timer_data = NULL;
static Handler g_timer_handler = NULL;

void SysTick_Handler() {
  if (g_timer_handler != NULL) {
    g_timer_handler(g_timer_data);
  }
}

uint32_t arm_hal_get_clock_rate() {
  return Chip_Clock_GetMainClockRate() / 10000;
}

// LPC-specific way of
void arm_hal_start_clock_us(uint32_t ticks_per_interrupt, Handler handler,
                            void* data) {
  g_timer_handler = handler;
  g_timer_data = data;

  // Setup the timer to fire interrupts at the requested interval.
  SysTick_Config(ticks_per_interrupt);

  // Enable interrupts
  __enable_irq();
}
