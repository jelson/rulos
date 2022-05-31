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

#include "core/arm-hal.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "core/logging.h"

// This is the entry point defined by the startup file after
// initialization is complete. We call the program's main().
//
// LPC startup files to SystemInit() first, but both STM32 and LPC
// startup files call _start last.
void _start() {
  extern int main(void);
  main();
}

rulos_irq_state_t hal_start_atomic() {
  rulos_irq_state_t old_interrupts = __get_PRIMASK();
  __disable_irq();
  return old_interrupts;
}

void hal_end_atomic(rulos_irq_state_t old_interrupts) {
  __set_PRIMASK(old_interrupts);
}

void hal_idle() {
  __WFI();
}

// timer_id is ignored for now; we just use the LPC SysTick clock,
// which is meant for use for a system clock because it doesn't have
// any pin inputs or outputs.
uint32_t hal_start_clock_us(uint32_t us, clock_handler_t handler, void *data,
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
  //
  // For jiffy periods less than 10ms, we multiply by the period
  // before dividing by 100, giving us extra precision without
  // overflow.
  const uint32_t clock_rate_div_10k = arm_hal_get_clock_rate() / 10000;
  uint32_t ticks_per_interrupt;
  if (us < 10000) {
    ticks_per_interrupt = (clock_rate_div_10k * us) / 100;
  } else {
    ticks_per_interrupt = clock_rate_div_10k * (us / 100);
  }

  // Ensure we're not trying to make the clock too long. For a 48mhz
  // crystal the maximum allowable jiffy clock is about 349ms.
  assert(ticks_per_interrupt < (1 << 24));

  arm_hal_start_clock_us(ticks_per_interrupt, handler, data);

  // Enable interrupts
  __enable_irq();

  // Reverse the process to get the (possibly rounded off)
  // microseconds per tick.
  uint32_t us_per_tick = ticks_per_interrupt;
  us_per_tick *= 100;
  us_per_tick /= clock_rate_div_10k;
  return us_per_tick;
}

bool hal_clock_interrupt_is_pending() {
  return (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0;
}
