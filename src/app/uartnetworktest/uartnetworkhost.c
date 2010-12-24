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

#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1

typedef struct {
	int fd;
	int send_idx;
	UartPreamble preamble;
	Message messageHeader;
	char send_buffer[250];
} Serial;

void serial_init(Serial *serial, const char *port_path)
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

	serial->send_idx = 0;

	serial->fd = fd;
}

void write_all(int fd, void *d, int len)
{
	int rc;
	rc = write(fd, d, len);
	assert(rc==len);
}

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

void serial_send_buffer(Serial *serial)
{
	uint8_t payload_len = serial->send_idx;
	uint8_t message_len = sizeof(Message) + payload_len;
	serial->preamble.p0 = UM_PREAMBLE0;
	serial->preamble.p1 = UM_PREAMBLE1;
	serial->preamble.dest_addr = AUDIO_ADDR;
	serial->preamble.len = message_len;
	serial->messageHeader.dest_port = UARTNETWORKTEST_PORT;
	serial->messageHeader.payload_len = payload_len;
	serial->messageHeader.checksum = 0;
	serial->messageHeader.checksum = net_compute_checksum(
		(char*) &serial->messageHeader, message_len);

	int len = sizeof(serial->preamble)+sizeof(serial->messageHeader)+payload_len;
	write_all(serial->fd, &serial->preamble, len);

	char buf[sizeof(serial->send_buffer)+1];
	memcpy(buf, serial->send_buffer, serial->send_idx);
	buf[serial->send_idx] = '\0';

	fprintf(stderr, "sent: \"%s\"\n", buf);
	display("SENT", (char*) &serial->preamble, len);

	serial->send_idx = 0;
}

void serial_loop(Serial *serial)
{
	int fd = serial->fd;
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
				serial_send_buffer(serial);
			}
			else
			{
				serial->send_buffer[serial->send_idx++] = c;
				if (serial->send_idx >= sizeof(serial->send_buffer))
				{
					serial_send_buffer(serial);
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

	Serial serial;
	serial_init(&serial, port_path);

	serial_loop(&serial);
	return -1;
}
