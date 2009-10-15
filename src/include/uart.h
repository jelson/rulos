#include "rocket.h"

typedef struct
{
	ByteQueue *q;
	Time reception_time_us;
} UartQueue_t;

// called by applications
UartQueue_t uart_queue_get();
void uart_init(uint16_t baud);
uint8_t uart_read(/* OUT */ uint8_t *c);
UartQueue_t uart_queue_get();

// called by the HAL
void uart_receive(char c); // character arrived on uart


