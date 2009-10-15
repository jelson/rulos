#include "rocket.h"
#include "uart.h"


// Global instance of the UART buffer state struct
char uart_queue_store[32];
UartQueue_t uart_queue_g;


// upcall from HAL
void uart_receive(char c)
{
	if (uart_queue_g.reception_time_us == 0) {
		uart_queue_g.reception_time_us = precise_clock_time_us();
	}
	ByteQueue_append(uart_queue_g.q, c);
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
	hal_start_atomic();
	uint8_t retval = ByteQueue_pop(uart_queue_g.q, c);
	hal_end_atomic();
	return retval;
}

void uart_init(uint16_t baud)
{
	// initialize the queue
	uart_queue_reset();

	hal_uart_init(baud);
}


