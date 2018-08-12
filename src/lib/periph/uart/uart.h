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

#ifndef __uart_h__
#define __uart_h__

#include "queue.h"
#include "heap.h"
#include "hal.h"

typedef struct {
	CharQueue *q;
	Time reception_time_us;
} UartQueue_t;

#define UART_QUEUE_LEN 32

typedef void (*UARTSendDoneFunc)(void *callback_data);

typedef struct
{
	UartHandler handler;
	uint8_t initted;

	// receive
	char recvQueueStore[UART_QUEUE_LEN];
	UartQueue_t recvQueue;

	// send
	const char *out_buf;
	uint8_t out_len;
	uint8_t out_n;
	UARTSendDoneFunc send_done_cb;
	void *send_done_cb_data;
} UartState_t;

///////////////// application API
void uart_init(UartState_t *uart, uint32_t baud, r_bool stop2, uint8_t uart_id);
r_bool uart_read(UartState_t *uart, char *c);
UartQueue_t *uart_recvq(UartState_t *uart);
void uart_reset_recvq(UartQueue_t *uq);

r_bool uart_send(UartState_t *uart, const char *c, uint8_t len, UARTSendDoneFunc, void *callback_data);
r_bool uart_busy(UartState_t *u);

void uart_debug_log(const char *m);
#ifdef ASSERT_TO_SERIAL
void uart_assert(uint16_t lineNum);
#endif

#endif

