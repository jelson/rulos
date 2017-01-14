/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "rulos.h"
#include "twi.h"
#include "7seg_panel.h"

#define HAVE_AUDIOBOARD_UART 0

#if !HAVE_AUDIOBOARD_UART
# define SYNCDEBUG() {}
#else

#define SYNCDEBUG()	syncdebug(0, 'T', __LINE__)

#define FOSC 8000000// Clock Speed
#define BAUD 38400
#define MYUBRR FOSC/16/BAUD-1

void serial_init()
{
	uart_init(RULOS_UART0, MYUBRR, TRUE);
}

void waitbusyuart()
{
	while (uart_busy(RULOS_UART0)) { }
}

void syncdebug(uint8_t spaces, char f, uint16_t line)
{
	char buf[16], hexbuf[6];
	int i;
	for (i=0; i<spaces; i++)
	{
		buf[i] = ' ';
	}
	buf[i++] = f;
	buf[i++] = ':';
	buf[i++] = '\0';
	if (f >= 'a')	// lowercase -> hex value; so sue me
	{
		debug_itoha(hexbuf, line);
	}
	else
	{
		itoda(hexbuf, line);
	}
	strcat(buf, hexbuf);
	strcat(buf, "\n");
	uart_send(RULOS_UART0, (char*) buf, strlen(buf), NULL, NULL);
	waitbusyuart();
}
#endif // HAVE_AUDIOBOARD_UART


void board_say(const char *s)
{
	SSBitmap bm[8];
	int i;

	ascii_to_bitmap_str(bm, 8, s);

	for (i = 0; i < 8; i++)
		program_board(i, bm);
}

static void recv_func(RecvSlot *recvSlot, uint8_t payload_size)
{
	char buf[payload_size+1];
	memcpy(buf, recvSlot->msg->data, payload_size);
	buf[payload_size] = '\0';
	LOGF((logfp, "Got network message [%d bytes]: '%s'\n", payload_size, buf));
	SYNCDEBUG();

	// display the first 8 chars to the leds
	buf[min(payload_size, 8)] = '\0';
	board_say(buf);
}


void test_netstack()
{
	Network net;
	char data[20];
	RecvSlot recvSlot;

	board_say(" rEAdy  ");

	init_clock(10000, TIMER1);

	recvSlot.func = recv_func;
	recvSlot.port = AUDIO_PORT;
	recvSlot.msg = (Message *) data;
	recvSlot.payload_capacity = sizeof(data) - sizeof(Message);
	recvSlot.msg_occupied = FALSE;

	SYNCDEBUG();
	init_twi_network(&net, 100, AUDIO_ADDR);
	net_bind_receiver(&net, &recvSlot);
	SYNCDEBUG();

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
}

int main()
{
	hal_init();
	hal_init_rocketpanel(bc_default);
#if HAVE_AUDIOBOARD_UART
	serial_init();
#endif

	test_netstack();
	return 0;
}
