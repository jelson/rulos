// NOTE: This file is not included in the default compile to prevent
// the UART interrupt handler (and all its consequent dependencies)
// from being implicitly included in all programs.  If writing a
// program that neds UART support, add uart.o to the list of
// application-specific objects.
#include "rocket.h"
#include "uart.h"

#ifndef SIM
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

// Global instance of the UART buffer state struct
char uart_queue_store[32];
UartQueue_t uart_queue_g;


// upcall from HAL
void uart_receive(char c)
{
	if (uart_queue_g.reception_time_us == 0) {
		uart_queue_g.reception_time_us = precise_clock_time_us();
	}
	LOGF((logfp, "uart_receive: got char at %d, msgtime=%d\n",
		  precise_clock_time_us(), uart_queue_g.reception_time_us))
	if (uart_queue_g.q != NULL)
	{
		ByteQueue_append(uart_queue_g.q, c);
	}
}

//////////////////////////////////////////////////////////


void uart_queue_reset()
{
	uart_queue_g.q = (ByteQueue *) uart_queue_store;
	ByteQueue_init(uart_queue_g.q, sizeof(uart_queue_store));
	uart_queue_g.reception_time_us = 0;
}

UartQueue_t *uart_queue_get()
{
	return &uart_queue_g;
}

uint8_t uart_read(uint8_t *c /* OUT */)
{
	uint8_t old_interrupts = hal_start_atomic();
	uint8_t retval = ByteQueue_pop(uart_queue_g.q, c);
	hal_end_atomic(old_interrupts);
	return retval;
}

void uart_init(uint16_t baud)
{
	// initialize the queue
	uart_queue_reset();

	hal_uart_init(baud);
}

////

// This really belongs in hardware.c, but moving it here makes it easy
// to avoid polluting non-uart-using progarms with the uart interrupt
// handler, simply by not linking this file.

#ifndef SIM
#if defined(MCUatmega328p)
ISR(USART_RX_vect)
{
	uart_receive(UDR0);
}
#elif defined(MCUatmega8)
ISR(USART_RXC_vect)
{
	uart_receive(UDR);
}
#else
#error need CPU love
#endif
#endif

