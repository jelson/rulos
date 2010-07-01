#include "rocket.h"

typedef struct UartState_s UartState_t;
#ifndef __UART_C__
extern UartState_t *RULOS_UART0;
#endif

typedef struct {
	ByteQueue *q;
	Time reception_time_us;
} UartQueue_t;

// called by the HAL
void uart_receive(UartState_t *uart, char c); // character arrived on uart

// called by applications
void uart_init(UartState_t *uart, uint16_t baud);
uint8_t uart_read(UartState_t *uart, uint8_t *c);
UartQueue_t *uart_recvq(UartState_t *uart);
void uart_reset_recvq(UartQueue_t *uq);
