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

#include "chip/arm/common/core/bits.h"
#include "core/hal.h"
#include "core/logging.h"

#undef TRUE
#undef FALSE
#include "chip.h"

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
  // The correct formula for ticks-per-clock-period is just the
  // frequency of the CPU in MHz times the desired period in
  // microseconds. However, we don't want to divide by the clock by 1M
  // because we might have a chip with fractional MHz in it and don't
  // want to lose the precision. So we divide by 10,000 (giving us
  // clock-frequency resolution down to 0.01 Mhz), and divide the
  // desired period by 100 before multiplying (giving us period
  // resolution down to 100 microseconds). Dividing the period first
  // prevents overflow of a 32-bit int for reasonable (< 1 second)
  // jiffy periods.
  const uint32_t clock_rate_div_10k = Chip_Clock_GetMainClockRate() / 10000;
  uint32_t ticks_per_interrupt = clock_rate_div_10k * (us / 100);

  // Ensure we're not trying to make the clock too long. For a 48mhz
  // crystal the maximum allowable jiffy clock is about 349ms.
  assert(ticks_per_interrupt < (1 << 24));

  g_timer_handler = handler;
  g_timer_data = data;

  // Setup the timer to fire interrupts at the requested interval.
  SysTick_Config(ticks_per_interrupt);

  // Enable interrupts
  __enable_irq();

  // Reverse the process to get the (possibly rounded off)
  // microseconds per tick.
  uint32_t us_per_tick = ticks_per_interrupt;
  us_per_tick *= 100;
  us_per_tick /= clock_rate_div_10k;
  return us_per_tick;
}

void hal_idle() { __WFI(); }

void hardware_assert(uint16_t line) {}
