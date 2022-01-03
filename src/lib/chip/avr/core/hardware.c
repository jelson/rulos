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

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include "core/hardware.h"

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>

#include "core/hal.h"
#include "core/logging.h"

uint8_t g_hal_initted = 0;

void rulos_hal_init() {
#ifdef MCUatmega1284p
  // Disable JTAG, which opens up PC2..PC5 as gpios (or their other features)
  MCUCR |= _BV(JTD);
  MCUCR |= _BV(JTD);
#endif  // MCUatmega1284p

  init_f_cpu();
  g_hal_initted = HAL_MAGIC;
}

//////////////////////////////////////////////////////////////////////////////

/**************************************************************/
/*			 Interrupt-driven senor input                   */
/**************************************************************/
// jonh hardcodes this for a specific interrupt line. Not sure
// yet how to generalize; will do when needed, I guess.

#if 0
Handler sensor_interrupt_handler;

void sensor_interrupt_register_handler(Handler handler)
{
	sensor_interrupt_handler = handler;
}

ISR(INT0_vect)
{
	sensor_interrupt_handler();
}
#endif

/*************************************************************************************/

// disable interrupts, and return true if interrupts had been enabled.
rulos_irq_state_t hal_start_atomic() {
  rulos_irq_state_t retval = SREG & _BV(SREG_I);
  cli();
  return retval;
}

// conditionally enable interrupts
void hal_end_atomic(rulos_irq_state_t interrupt_flag) {
  if (interrupt_flag) {
    sei();
  }
}

void hal_idle() {
  // just busy-wait on microcontroller.
}

void hal_deep_sleep() {
#ifdef PRR
  uint8_t prr_old = PRR;
  PRR = 0xff;
#endif
#ifdef PRR0
  uint8_t prr0_old = PRR0;
  PRR1 = 0xff;
#endif
#ifdef PRR1
  uint8_t prr1_old = PRR1;
  PRR1 = 0xff;
#endif
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

#if defined(BODS) && defined(BODSE)
  sleep_bod_disable();
#endif
  sei();
  sleep_cpu();
  sleep_disable();

  // Cpu has re-awakened!
#ifdef PRR
  PRR = prr_old;
#endif
#ifdef PRR0
  PRR0 = prr0_old;
#endif
#ifdef PRR1
  PRR1 = prr1_old;
#endif
  hal_end_atomic(old_interrupts);
}

#ifdef ASSERT_TO_BOARD
#include "display_controller.h"
#endif

void avr_assert(const uint16_t line) {
  LOG("assertion failed: line %d", line);

#ifdef ASSERT_TO_BOARD
  board_debug_msg(line);
#endif
#ifdef ASSERT_CUSTOM
  ASSERT_CUSTOM(line);
#endif

  while (1) {
  }
}
