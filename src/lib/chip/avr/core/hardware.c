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

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>

#include "core/hal.h"
#include "hardware.h"


uint8_t hal_initted = 0;

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
rulos_irq_state_t hal_start_atomic()
{
	rulos_irq_state_t retval = SREG & _BV(SREG_I);
	cli();
	return retval;
}

// conditionally enable interrupts
void hal_end_atomic(rulos_irq_state_t interrupt_flag)
{
	if (interrupt_flag)
		sei();
}

void hal_idle()
{
	// just busy-wait on microcontroller.
}

void hal_deep_sleep()
{
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
	sleep_enable();

#if defined(BODS) && defined(BODSE)
	sleep_bod_disable();
#endif
	sei();
	sleep_cpu();

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
	
}

void hal_init()
{
#ifdef MCUatmega1284p
// Disable JTAG, which opens up PC2..PC5 as gpios (or their other features)
	MCUCR |= _BV(JTD);
	MCUCR |= _BV(JTD);
#endif // MCUatmega1284p

	init_f_cpu();
	hal_initted = HAL_MAGIC;
}

#ifdef ASSERT_TO_SERIAL
# include "periph/uart/uart.h"
#endif

#ifdef ASSERT_TO_BOARD
# include "display_controller.h"
#endif

void hardware_assert(uint16_t line)
{
#ifdef ASSERT_TO_BOARD
	board_debug_msg(line);
#endif
#ifdef ASSERT_TO_SERIAL
	uart_assert(line);
#endif
#ifdef ASSERT_CUSTOM
ASSERT_CUSTOM(line);
#endif

	while (1) { }
}

