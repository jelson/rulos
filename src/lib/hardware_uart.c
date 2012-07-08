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

#include <rocket.h>
#include <mark_point.h>

void audioled_set(r_bool red, r_bool yellow);

/*
 * hardware_uart.c: UART
 *
 * This file is not compiled by the simulator.
 */

#include <stdbool.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "uart.h"
#include "hal.h"


uint16_t baud_to_ubrr(uint32_t baud)
{
	return ((uint32_t) hardware_f_cpu) / 16 / baud - 1;
}

UartHandler* g_uart_handler[2] = {NULL, NULL};

#if defined(MCU8_line)

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
# define _USBS     USBS
# define _USART_RXC_vect USART_RXC_vect
# define _USART_UDRE_vect USART_UDRE_vect
#define hal_uart_init_name hal_uart_init0
#define enable_sendready_interrupt_name enable_sendready_interrupt0
#define hal_uart_start_send_name hal_uart_start_send0
#define _handle_recv_ready_name _handle_recv_ready0
#define _handle_send_ready_name _handle_send_ready0
#define hal_uart_start_send_name hal_uart_start_send0
#define hal_uart_sync_send_name hal_uart_sync_send0
#define UARTID 0
#define HAVE_UARTID0 1
#include "hardware_uart.ch"

#elif defined(MCU328_line) || defined(MCU1284_line)

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
# define _USBS     USBS0
# define _USART_RXC_vect USART0_RX_vect
# define _USART_UDRE_vect USART0_UDRE_vect
#define hal_uart_init_name hal_uart_init0
#define enable_sendready_interrupt_name enable_sendready_interrupt0
#define hal_uart_start_send_name hal_uart_start_send0
#define _handle_recv_ready_name _handle_recv_ready0
#define _handle_send_ready_name _handle_send_ready0
#define hal_uart_start_send_name hal_uart_start_send0
#define hal_uart_sync_send_name hal_uart_sync_send0
#define UARTID 0
#define HAVE_UARTID0 1
#include "hardware_uart.ch"

#undef _UBRRH
#undef _UBRRL
#undef _UCSRA
#undef _UCSRB
#undef _UCSRC
#undef _RXEN
#undef _RXCIE
#undef _TXEN
#undef _TXCIE
#undef _UCSZ0
#undef _UCSZ1
#undef _UDR
#undef _UDRE
#undef _UDRIE
#undef _USBS
#undef _USART_RXC_vect
#undef _USART_UDRE_vect
#undef hal_uart_init_name
#undef enable_sendready_interrupt_name
#undef hal_uart_start_send_name
#undef _handle_recv_ready_name
#undef _handle_send_ready_name
#undef hal_uart_start_send_name
#undef hal_uart_sync_send_name
#undef UARTID

# define _UBRRH    UBRR1H
# define _UBRRL    UBRR1L
# define _UCSRA    UCSR1A
# define _UCSRB    UCSR1B
# define _UCSRC    UCSR1C
# define _RXEN     RXEN1
# define _RXCIE    RXCIE1
# define _TXEN     TXEN1
# define _TXCIE    TXCIE1
# define _UCSZ0    UCSZ10
# define _UCSZ1    UCSZ11
# define _UDR      UDR1
# define _UDRE     UDRE1
# define _UDRIE    UDRIE1
# define _USBS     USBS1
# define _USART_RXC_vect USART1_RX_vect
# define _USART_UDRE_vect USART1_UDRE_vect
#define hal_uart_init_name hal_uart_init1
#define enable_sendready_interrupt_name enable_sendready_interrupt1
#define hal_uart_start_send_name hal_uart_start_send1
#define _handle_recv_ready_name _handle_recv_ready1
#define _handle_send_ready_name _handle_send_ready1
#define hal_uart_start_send_name hal_uart_start_send1
#define hal_uart_sync_send_name hal_uart_sync_send1
#define UARTID 1
#define HAVE_UARTID1 1
#include "hardware_uart.ch"

#else
# error Hardware-specific UART code needs love
#endif

void hal_uart_init(UartHandler* handler, uint32_t baud, r_bool stop2, uint8_t uart_id)
{
	handler->uart_id = uart_id;
	switch (uart_id) {
#if HAVE_UARTID0
	case 0: hal_uart_init0(handler, baud, stop2, uart_id); return;
#endif
#if HAVE_UARTID1
	case 1: hal_uart_init1(handler, baud, stop2, uart_id); return;
#endif
	default:
		assert(false);
	}
}

void hal_uart_start_send(UartHandler* handler)
{
	switch (handler->uart_id) {
#if HAVE_UARTID0
	case 0: hal_uart_start_send0(); return;
#endif
#if HAVE_UARTID1
	case 1: hal_uart_start_send1(); return;
#endif
	default:
		assert(false);
	}
}

void hal_uart_sync_send(UartHandler* handler, char *s, uint8_t len)
{
	switch (handler->uart_id) {
#if HAVE_UARTID0
	case 0: hal_uart_sync_send0(s, len); return;
#endif
#if HAVE_UARTID1
	case 1: hal_uart_sync_send1(s, len); return;
#endif
	default:
		assert(false);
	}
}

#ifdef ASSERT_TO_SERIAL
	// Assumes that UART0 is always the debug uart.

void uart_assert(uint16_t lineNum)
{
	char buf[9];
	buf[0] = 'a';
	buf[1] = 's';
	buf[2] = 'r';
	int_to_string2(&buf[3], 5, 0, lineNum);
	buf[8] = 0;

	cli();
	hal_uart_sync_send0(buf, sizeof(buf));
}

#endif
