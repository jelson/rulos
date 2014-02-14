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
#include "uart_net_media_preamble.h"
#include "message.h"
#include "network_ports.h"
#include "net_compute_checksum.h"
#include "host_uart_network.h"

#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1

void host_uart_network_init(HostUartNetwork *hun, const char *port_path)
{
	int fd = open(port_path, O_RDWR|O_NOCTTY);
	if (fd<0) {
		perror(port_path);
		assert(0);
	}

	struct {
		int magic0;
		struct termios termios;
		char overflow_buffer[3];
			// there's some disagreement between header files and tcgetattr's
			// idea of termios. magics let me detect it;
			// this buffer gives enough space to work around it. Uck!
		int magic1;
	} check;
	struct termios *termios_p = &check.termios;
	check.magic0 = 0xdeadbeef;
	check.magic1 = 0xdeadbeef;

	int rc;
	rc = tcgetattr(fd, termios_p);
	assert(rc==0);

	speed_t baud = B38400;
	rc = cfsetispeed(termios_p, baud);
	assert(rc==0);
	rc = cfsetospeed(termios_p, baud);
	assert(rc==0);
	termios_p->c_cflag &= (~CRTSCTS);

	printf("CRTSCTS = %d\n", CRTSCTS);

	rc = tcsetattr(fd, TCSANOW, termios_p);
	assert(rc==0);

	rc = tcflush(fd, TCIOFLUSH);
	assert(rc==0);

	rc = tcgetattr(fd, termios_p);
	assert(rc==0);

	hun->fd = fd;
}

static void write_all(int fd, void *d, int len)
{
	int rc;
	rc = write(fd, d, len);
	assert(rc==len);
}

void host_uart_network_send_buffer(HostUartNetwork *hun, uint8_t addr, uint8_t port, uint8_t payload_len)
{
	uint8_t message_len = sizeof(Message) + payload_len;
	HostUartFrame *frame = &hun->send_frame;
	frame->preamble.p0 = UM_PREAMBLE0;
	frame->preamble.p1 = UM_PREAMBLE1;
	frame->preamble.dest_addr = addr;
	frame->preamble.len = message_len;
	frame->messageHeader.dest_port = port;
	frame->messageHeader.payload_len = payload_len;
	frame->messageHeader.checksum = 0;
	frame->messageHeader.checksum = net_compute_checksum(
		(char*) &frame->messageHeader, message_len);

	int packet_len = sizeof(frame->preamble)+sizeof(frame->messageHeader)+payload_len;
	write_all(hun->fd, &frame->preamble, packet_len);
}

void host_uart_receive(HostUartNetwork *hun)
{
	while (1)
	{
		char c;
		int rc;

		rc = read(hun->fd, &c, 1);
		if (rc!=1) { break; }
		if (c!=UM_PREAMBLE0)
		{
			continue; // wait for a preamble
		}

		rc = read(hun->fd, &c, 1);
		if (rc!=1) { break; }
		if (c!=UM_PREAMBLE1)
		{
			continue; // wait for a complete preamble
		}

		// now that we have valid preamble, we read the rest of the uart
		// header and all the specified len; no need to resync
		int len = sizeof(&hun->recv_frame.preamble) - 2;
		rc = read(hun->fd, &hun->recv_frame.preamble.dest_addr, len);
		if (rc!=len) { break; }

		len = hun->recv_frame.preamble.len;
		assert(len < sizeof(Message)+sizeof(hun->recv_frame.buffer));
		rc = read(hun->fd, &hun->recv_frame.messageHeader, len);
		if (rc!=len) { break; }

		// here is where we'd dispatch the packet
		fprintf(stderr, "Recvd packet addr 0x%2x port 0x%2x payload_len 0x%2x\n",
			hun->recv_frame.preamble.dest_addr,
			hun->recv_frame.messageHeader.dest_port,
			hun->recv_frame.messageHeader.payload_len);
		break;
	}
}
