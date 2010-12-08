#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <asm/ioctls.h>
#include <linux/serial.h>
#include <assert.h>

#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1


int main(int argc, const char **argv)
{
	int rc;
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

	int fd = open(port_path, O_RDWR|O_NOCTTY);
	if (fd<0) {
		perror(port_path);
		assert(0);
	}


#if 0
	// via setserial.c source
	struct serial_struct serinfo;
	rc = ioctl(fd, TIOCGSERIAL, &serinfo);
	assert(rc==0);

	// https://trac.v2.nl/wiki/tracking/hexamite
	//serinfo.baud_base = 4500000;
	//serinfo.custom_divisor = 18;
#if 0
	serinfo.flags |= ASYNC_SPD_CUST;
	serinfo.baud_base = 24000000;
	serinfo.custom_divisor = 48;	// 500000
//	serinfo.custom_divisor = 96;	// 250000
//	serinfo.custom_divisor = 336;	// 71428
#else
	serinfo.flags &= ~ASYNC_SPD_CUST;
	serinfo.baud_base = 24000000;
	serinfo.custom_divisor = 1;
#endif
	
	rc = ioctl(fd, TIOCSSERIAL, &serinfo);
	assert(rc==0);

	rc = ioctl(fd, TIOCGSERIAL, &serinfo);
	assert(rc==0);
	printf("baud: %d / %d = %d\n",
		serinfo.baud_base,
		serinfo.custom_divisor,
		serinfo.baud_base / serinfo.custom_divisor);
	fflush(stdout);

	struct termios oldtio, newtio;
	rc = tcgetattr(fd, &oldtio);
	assert(rc==0);
	// B38400 -> use serial speed provided above.
	// CS8 | CSTOPB | 0 | 0 -> 8N1
	newtio.c_cflag = B38400 | CRTSCTS | CS8 | CSTOPB | 0 | 0 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	rc = tcflush(fd, TCIFLUSH);
	assert(rc==0);
	rc = tcsetattr(fd, TCSANOW, &newtio);
	assert(rc==0);
#else
	struct termios termios_p;

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
#endif

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
			write(fd, &c, 1);
			assert(rc==1);
		}
		if (FD_ISSET(fd, &readfds))
		{
			char c;
			rc = read(fd, &c, 1);
			assert(rc==1);
			write(1, &c, 1);
			assert(rc==1);
		}
	}

#if 0
	while (1)
	{
		char c;
		rc = read(fd, &c, 1);
		if (rc<0)
		{
			perror("read");
			assert(0);
		}
		else if (rc==0)
		{
			break;
		}
		putc(':', stdout);
		putc(c, stdout);
		fflush(stdout);
	}

#endif
}
