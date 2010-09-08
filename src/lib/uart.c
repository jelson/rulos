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

// NOTE: This file is not included in the default compile to prevent
// the UART interrupt handler (and all its consequent dependencies)
// from being implicitly included in all programs.  If writing a
// program that neds UART support, add uart.o to the list of
// application-specific objects.
#define __UART_C__
#include "rocket.h"

#ifndef UART_QUEUE_LEN
#define UART_QUEUE_LEN 32
#endif


struct UartState_s
{
	uint8_t initted;

	// receive
	char recvQueueStore[UART_QUEUE_LEN];
	UartQueue_t recvQueue;

	// send
	char *out_buf;
	uint8_t out_len;
	uint8_t out_n;
	UARTSendDoneFunc send_done_cb;
	void *send_done_cb_data;
};

// Global instance of the UART buffer state struct
UartState_t uart0_g = {0};
UartState_t *RULOS_UART0 = &uart0_g;


// Upcall from HAL when new data arrives.  Happens at interrupt time.
void _uart_receive(UartState_t *u, char c)
{
	if (!u->initted)
		return;

	if (u->recvQueue.reception_time_us == 0) {
		u->recvQueue.reception_time_us = precise_clock_time_us();
	}

	LOGF((logfp, "uart_receive: got char at %d, msgtime=%d\n",
		  precise_clock_time_us(), u->recvQueue.reception_time_us));

	// safe because we're in interrupt time.
	CharQueue_append(u->recvQueue.q, c);
}

// Upcall from hal when the next byte is needed for a send.  Happens
// at interrupt time.
r_bool _uart_get_next_character(UartState_t *u, char *c /* OUT */)
{
	assert(u != NULL);

	if (u->out_buf == NULL)
	{
		return FALSE;
	}

	*c = u->out_buf[u->out_n++];

	if (u->out_n == u->out_len)
	{
		if (u->send_done_cb) {
			//schedule_now(u->send_done_cb, u->send_done_cb_data);
			// FIXME!
			u->send_done_cb(u->send_done_cb_data);
		}
		u->out_buf = NULL;
		u->send_done_cb = NULL;
		u->send_done_cb_data = NULL;
	}

	return TRUE;
}


//////////////////////////////////////////////////////////


r_bool uart_read(UartState_t *u, char *c /* OUT */)
{
	uint8_t old_interrupts = hal_start_atomic();
	r_bool retval = CharQueue_pop(u->recvQueue.q, c);
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
	CharQueue_clear(uq->q);
}

r_bool uart_send(UartState_t *u, char *c, uint8_t len,
				 UARTSendDoneFunc callback, void *callback_data)
{
	assert(u->initted);

	if (u->out_buf != NULL || len == 0)
		return FALSE;

	u->out_buf = c;
	u->out_len = len;
	u->out_n = 0;
	u->send_done_cb = callback;
	u->send_done_cb_data = callback_data;

	hal_uart_start_send(u);
	return TRUE;
}


void uart_init(UartState_t *u, uint16_t baud)
{
	// initialize the queue
	u->recvQueue.q = (CharQueue *) u->recvQueueStore;
	CharQueue_init(u->recvQueue.q, sizeof(u->recvQueueStore));
	u->recvQueue.reception_time_us = 0;
	hal_uart_init(u, baud);
	u->initted = TRUE;
}

