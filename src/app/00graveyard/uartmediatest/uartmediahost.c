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

#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1

typedef struct {
	int fd;
	int send_idx;
	char send_buffer[250];
	UartPreamble preamble;
} Serial;

void serial_init(Serial *serial, const char *port_path)
{
	int fd = open(port_path, O_RDWR|O_NOCTTY);
	if (fd<0) {
		perror(port_path);
		assert(0);
	}

	struct termios termios_p;

	int rc;
	rc = tcgetattr(fd, &termios_p);
	assert(rc==0);

	speed_t baud = B38400;
	rc = cfsetispeed(&termios_p, baud);
	assert(rc==0);
	rc = cfsetospeed(&termios_p, baud);
	assert(rc==0);
	termios_p.c_cflag &= (~CRTSCTS);

	printf("CRTSCTS = %d\n", CRTSCTS);

	rc = tcsetattr(fd, TCSANOW, &termios_p);
	assert(rc==0);

	rc = tcflush(fd, TCIOFLUSH);
	assert(rc==0);

	rc = tcgetattr(fd, &termios_p);
	assert(rc==0);

	serial->send_idx = 0;

	serial->fd = fd;
}

void serial_send_buffer(Serial *serial)
{
	serial->preamble.p0 = UM_PREAMBLE0;
	serial->preamble.p1 = UM_PREAMBLE1;
	serial->preamble.dest_addr = 0x01;
	serial->preamble.len = serial->send_idx;
	int rc;
	rc = write(serial->fd, &serial->preamble, sizeof(serial->preamble));
	assert(rc==sizeof(serial->preamble));
	rc = write(serial->fd, serial->send_buffer, serial->send_idx);
	assert(rc==serial->send_idx);

	char buf[sizeof(serial->send_buffer)+1];
	memcpy(buf, serial->send_buffer, serial->send_idx);
	buf[serial->send_idx] = '\0';
	fprintf(stderr, "sent: \"%s\"\n", buf);

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
			fprintf(stderr, "RECV [%2x] '%c'\n",
				c & 0xff, (c>=' ' && c<127) ? c : '_');
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
