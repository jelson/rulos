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
	ByteQueue *q;
	Time reception_time_us;
} UartQueue_t;

// called by the HAL
void _uart_receive(UartState_t *uart, char c); // character arrived on uart
r_bool _uart_get_next_character(UartState_t *u, char *c /* OUT */); // need next character to send


// called by applications
void uart_init(UartState_t *uart, uint16_t baud);
uint8_t uart_read(UartState_t *uart, uint8_t *c);
UartQueue_t *uart_recvq(UartState_t *uart);
void uart_reset_recvq(UartQueue_t *uq);

typedef void (*UARTSendDoneFunc)(void *callback_data);
r_bool uart_send(UartState_t *uart, char *c, uint8_t len, UARTSendDoneFunc, void *callback_data);

#endif

