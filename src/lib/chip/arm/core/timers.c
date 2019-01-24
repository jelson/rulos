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

#include <inttypes.h>

#include "chip/arm/core/bits.h"
#include "core/hal.h"
#include "core/logging.h"

#undef TRUE
#undef FALSE
#include "chip/arm/lpc_chip_11cxx_lib/chip.h"

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

// timer_id is ignored for now; we just use the LPC SysTick clock,
// which is meant for use for a system clock because it doesn't have
// any pin inputs or outputs.
uint32_t hal_start_clock_us(uint32_t us, Handler handler, void* data,
                            uint8_t timer_id) {
  const uint32_t clock_rate = Chip_Clock_GetMainClockRate();
  uint64_t ticks_per_interrupt = clock_rate;
  ticks_per_interrupt *= (uint64_t)us;
  ticks_per_interrupt /= (uint64_t)1000000;

  // Ensure we're not trying to make the clock too long. For a 48mhz
  // crystal the maximum allowable jiffy clock is about 349ms.
  assert(ticks_per_interrupt < (1 << 24));

  g_timer_handler = handler;
  g_timer_data = data;

  // Configure systick's clock source and to generate interrupts.
  reg32_set(&SysTick->CTRL,
            SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk);

  // Set initial value and rollover value.
  SysTick->VAL = 0;
  SysTick->LOAD = ticks_per_interrupt;

  // Enable systick timer
  reg32_set(&SysTick->CTRL, SysTick_CTRL_ENABLE_Msk);

  // Enable interrupts
  __enable_irq();

  uint64_t us_per_tick = ticks_per_interrupt;
  us_per_tick *= 1000000;
  us_per_tick /= clock_rate;
  return us_per_tick;
}

void hal_idle() { __WFI(); }

void hardware_assert(uint16_t line) {}
