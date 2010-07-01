// NOTE: This file is not included in the default compile to prevent
// the UART interrupt handler (and all its consequent dependencies)
// from being implicitly included in all programs.  If writing a
// program that neds UART support, add uart.o to the list of
// application-specific objects.
#define __UART_C__
#include "rocket.h"
#include "uart.h"

#ifndef SIM
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

#ifndef UART_QUEUE_LEN
#define UART_QUEUE_LEN 32
#endif

struct UartState_s
{
	uint8_t initted;
	char recvQueueStore[UART_QUEUE_LEN];
	UartQueue_t recvQueue;
	char *out_buf;
	uint8_t out_len;
	uint8_t out_n;
};

// Global instance of the UART buffer state struct
UartState_t uart0_g = {0};
UartState_t *RULOS_UART0 = &uart0_g;


// upcall from HAL.  Happens at interrupt time.
void uart_receive(UartState_t *u, char c)
{
	if (!u->initted)
		return;

	if (u->recvQueue.reception_time_us == 0) {
		u->recvQueue.reception_time_us = precise_clock_time_us();
	}

	LOGF((logfp, "uart_receive: got char at %d, msgtime=%d\n",
		  precise_clock_time_us(), u->recvQueue.reception_time_us));

	// safe because we're in interrupt time.
	ByteQueue_append(u->recvQueue.q, c);
}

//////////////////////////////////////////////////////////


uint8_t uart_read(UartState_t *u, uint8_t *c /* OUT */)
{
	uint8_t old_interrupts = hal_start_atomic();
	uint8_t retval = ByteQueue_pop(u->recvQueue.q, c);
	hal_end_atomic(old_interrupts);
	return retval;
}

UartQueue_t *uart_recvq(UartState_t *u)
{
	if (!u->initted)
		return NULL;

	return &u->recvQueue;
}

void uart_reset_recvq(UartQueue_t *uq)
{
	uq->reception_time_us = 0;
	ByteQueue_clear(uq->q);
}


void uart_init(UartState_t *u, uint16_t baud)
{
	// initialize the queue
	u->recvQueue.q = (ByteQueue *) u->recvQueueStore;
	ByteQueue_init(u->recvQueue.q, sizeof(u->recvQueueStore));
	u->recvQueue.reception_time_us = 0;
	hal_uart_init(baud); // parameterize this when we have 2 uarts
	u->initted = TRUE;
}





////

// This really belongs in hardware.c, but moving it here makes it easy
// to avoid polluting non-uart-using programs with the uart interrupt
// handler, simply by not linking this file.

#ifndef SIM
#if defined(MCUatmega328p)
ISR(USART_RX_vect)
{
	uart_receive(RULOS_UART0, UDR0);
}
#elif defined(MCUatmega8)
ISR(USART_RXC_vect)
{
	uart_receive(RULOS_UART0, UDR);
}
#else
#error need CPU love
#endif
#endif

