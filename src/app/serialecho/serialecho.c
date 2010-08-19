#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "uart.h"


int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);

	// start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	// start the uart running at 500k baud (assumes 8mhz clock: fixme)
	uart_init(RULOS_UART0, 0);
	UartQueue_t *recvQueue = uart_recvq(RULOS_UART0);

	while (1) {
		uint8_t uartSendBuf;

		if (ByteQueue_pop(recvQueue->q, &uartSendBuf)) {
			uartSendBuf++;
			uart_send(RULOS_UART0, &uartSendBuf, 1, NULL, NULL);
		}
	}
}

