#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <asm/ioctls.h>
#include <linux/serial.h>
#include <assert.h>
#include "periph/uart/uart_net_media_preamble.h"
#include "core/message.h"
#include "core/network_ports.h"
#include "core/net_compute_checksum.h"
#include "host/host_uart_network.h"

#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1


void display(char *token, char *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		char c = buf[i];
		fprintf(stderr, "%s [%2x] '%c'\n",
			token, c & 0xff, (c>=' ' && c<127) ? c : '_');
	}
}

void serial_loop(HostUartNetwork *hun)
{
	int fd = hun->fd;
	int send_idx;
	while (1)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(fd, &readfds);
		int rc = select(fd+1, &readfds, NULL, NULL, NULL);
		if (rc<0) {
			perror("select");
			break;
		} else if (rc==0)
		{
			continue;
		}

		if (FD_ISSET(0, &readfds))
		{
			char c;
			rc = read(0, &c, 1);
			assert(rc==1);
			if (c=='\n')
			{
				host_uart_network_send_buffer(
					hun, AUDIO_ADDR, UARTNETWORKTEST_PORT, send_idx);
				send_idx = 0;
			}
			else
			{
				hun->send_frame.buffer[send_idx++] = c;
				if (send_idx >= sizeof(hun->send_frame.buffer))
				{
					host_uart_network_send_buffer(
						hun, AUDIO_ADDR, UARTNETWORKTEST_PORT, send_idx);
				}
			}
		}
		if (FD_ISSET(fd, &readfds))
		{
			char c;
			rc = read(fd, &c, 1);
			assert(rc==1);
			display("RECV", &c, 1);
		}
	}
}

int main(int argc, const char **argv)
{
	const char *port_path;
	if (argc==2)
	{
		port_path = argv[1];
	}
	else if (argc==1)
	{
		port_path = "/dev/ttyUSB0";
	}
	else
	{
		assert(FALSE);
	}

	HostUartNetwork hun;
	host_uart_network_init(&hun, port_path);

	serial_loop(&hun);
	return -1;
}
