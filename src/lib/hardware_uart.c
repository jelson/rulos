/*
 * hardware_uart.c: UART
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "uart.h"
#include "hal.h"


//////////////////////////////////////////////////////////////////////////////

//// uart stuff

#if defined(MCUatmega8)
# define _UBRRH    UBRRH
# define _UBRRL    UBRRL
# define _UCSRA    UCSRA
# define _UCSRB    UCSRB
# define _UCSRC    UCSRC
# define _RXEN     RXEN
# define _RXCIE    RXCIE
# define _UCSZ0    UCSZ0
# define _UCSZ1    UCSZ1
#elif defined(MCUatmega328p)
# define _UBRRH    UBRR0H
# define _UBRRL    UBRR0L
# define _UCSRA    UCSR0A
# define _UCSRB    UCSR0B
# define _UCSRC    UCSR0C
# define _RXEN     RXEN0
# define _RXCIE    RXCIE0
# define _UCSZ0    UCSZ00
# define _UCSZ1    UCSZ01
#else
# error Hardware-specific UART code needs love
#endif

void hal_uart_init(uint16_t baud)
{
	// disable interrupts
	cli();

	// set baud rate
	_UBRRH = (unsigned char) baud >> 8;
	_UBRRL = (unsigned char) baud;

	// enable receiver and receiver interrupts
	_UCSRB = _BV(_RXEN) | _BV(_RXCIE);

	// set frame format: async, 8 bit data, 1 stop  bit, no parity
	_UCSRC =  _BV(_UCSZ1) | _BV(_UCSZ0)
#ifdef MCUatmega8
	  | _BV(URSEL)
#endif
	  ;

	// enable interrupts, whether or not they'd been previously enabled
	sei();
}

