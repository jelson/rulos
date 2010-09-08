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


#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif


typedef struct UartState_s UartState_t;
#ifndef __UART_C__
extern UartState_t *RULOS_UART0;
#endif

typedef struct {
	CharQueue *q;
	Time reception_time_us;
} UartQueue_t;

///////////////// called by the HAL
// character arrived on uart
void _uart_receive(UartState_t *uart, char c);

// need next character to send
r_bool _uart_get_next_character(UartState_t *u, char *c /* OUT */);


///////////////// called by applications
void uart_init(UartState_t *uart, uint16_t baud);
r_bool uart_read(UartState_t *uart, char *c);
UartQueue_t *uart_recvq(UartState_t *uart);
void uart_reset_recvq(UartQueue_t *uq);

typedef void (*UARTSendDoneFunc)(void *callback_data);
r_bool uart_send(UartState_t *uart, char *c, uint8_t len, UARTSendDoneFunc, void *callback_data);

#endif

