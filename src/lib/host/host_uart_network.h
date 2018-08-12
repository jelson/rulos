#ifndef _HOST_UART_NETWORK_H
#define _HOST_UART_NETWORK_H

#include "periph/uart/uart_net_media.h"
// host-side uart network stack

typedef struct {
	UartPreamble preamble;
	Message messageHeader;
	char buffer[250];
		// 250 is the most bytes anybody will ever need. :v)
} HostUartFrame;

typedef struct {
	int fd;
	HostUartFrame recv_frame;
	HostUartFrame send_frame;
} HostUartNetwork;

void host_uart_network_init(HostUartNetwork *hun, const char *port_path);
void host_uart_network_send_buffer(HostUartNetwork *hun, uint8_t addr, uint8_t port, uint8_t len);
void host_uart_receive(HostUartNetwork *hun);

#endif // _HOST_UART_NETWORK_H
