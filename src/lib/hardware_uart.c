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



// NOTE: These functions currently statically use Uart0 at
// compile-time using the constants below.  However, all the plumbing
// is here to support multiple uarts; each function is already
// parameterized with the uart struct.  To support multiple uarts,
// compare the UartState passed in to each function and change the
// register names conditionally.

#if defined(MCUatmega8)
# define _UBRRH    UBRRH
# define _UBRRL    UBRRL
# define _UCSRA    UCSRA
# define _UCSRB    UCSRB
# define _UCSRC    UCSRC
# define _RXEN     RXEN
# define _RXCIE    RXCIE
# define _TXEN     TXEN
# define _TXCIE    TXCIE
# define _UCSZ0    UCSZ0
# define _UCSZ1    UCSZ1
# define _UDR      UDR
# define _UDRE     UDRE
# define _UDRIE    UDRIE
#elif defined(MCUatmega328p)
# define _UBRRH    UBRR0H
# define _UBRRL    UBRR0L
# define _UCSRA    UCSR0A
# define _UCSRB    UCSR0B
# define _UCSRC    UCSR0C
# define _RXEN     RXEN0
# define _RXCIE    RXCIE0
# define _TXEN     TXEN0
# define _TXCIE    TXCIE0
# define _UCSZ0    UCSZ00
# define _UCSZ1    UCSZ01
# define _UDR      UDR0
# define _UDRE     UDRE0
# define _UDRIE    UDRIE0
#else
# error Hardware-specific UART code needs love
#endif


void hal_uart_init(UartState_t *s, uint16_t baud)
{
	// disable interrupts
	cli();

	// set baud rate
	_UBRRH = (unsigned char) baud >> 8;
	_UBRRL = (unsigned char) baud;

	_UCSRB =
		_BV(_TXEN)  | // enable transmitter
		_BV(_RXEN)  |  // enable receiver
		_BV(_RXCIE)   // enable receiver interrupt
		;

	// set frame format: async, 8 bit data, 1 stop bit, no parity
	_UCSRC =  _BV(_UCSZ1) | _BV(_UCSZ0)
#ifdef MCUatmega8
	  | _BV(URSEL)
#endif
	  ;

	// enable interrupts, whether or not they'd been previously enabled
	sei();
}

static inline void enable_sendready_interrupt(uint8_t enable)
{
	if (enable)
		reg_set(&_UCSRB, _UDRIE);
	else
		reg_clr(&_UCSRB, _UDRIE);
}



// Runs in user context.
void hal_uart_start_send(UartState_t *u)
{
	// Just enable the send-ready interrupt.  It will fire as soon as
	// the sender register is free.
	uint8_t old_interrupts = hal_start_atomic();
	enable_sendready_interrupt(TRUE);
	hal_end_atomic(old_interrupts);
}


// Runs in interrupt context.
void handle_send_ready(UartState_t *u)
{
	char c;

	// If there is still data remaining to send, send it.  Otherwise,
	// disable the send-ready interrupt.
	if (_uart_get_next_character(u, &c))
		_UDR = c;
	else
		enable_sendready_interrupt(FALSE);
}


#if defined(MCUatmega8)
ISR(USART_RXC_vect)
{
	_uart_receive(RULOS_UART0, UDR);
}

#elif defined(MCUatmega328p)
ISR(USART_RX_vect)
{
	_uart_receive(RULOS_UART0, UDR0);
}
ISR(USART_UDRE_vect)
{
	handle_send_ready(RULOS_UART0);
}
#else
#error need CPU love
#endif
