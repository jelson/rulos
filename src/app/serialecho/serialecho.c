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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rocket.h"
#include "uart.h"
#include "spiflash.h"

#include "hardware.h"	// sorry. Blinky LED.

//////////////////////////////////////////////////////////////////////////////

#include "hardware.h"	// sorry. Blinky LED.

typedef struct {
	Activation act;
	int state;
} BlinkAct;

void blink_once(BlinkAct *ba)
{
	ba->state = (ba->state==0);
	gpio_set_or_clr(GPIO_C5, (ba->state!=0));
}

void blink_update(Activation *act)
{
	BlinkAct *ba = (BlinkAct*) act;
	blink_once(ba);
	schedule_us(100000, &ba->act);
}

void blink_init(BlinkAct *ba, r_bool run)
{
	ba->act.func = blink_update;
	gpio_make_output(GPIO_C5);
	if (run)
	{
		schedule_us(50000, &ba->act);
	}
}

//////////////////////////////////////////////////////////////////////////////

typedef void (cmd_cb_f)(const char *cmd);

typedef struct {
	Activation act;
	UartQueue_t *recvQueue;
	char cmd[80];
	char *cmd_ptr;
	cmd_cb_f *cmd_cb;
} SerialCmdAct;

void serial_echo_update(Activation *act)
{
	SerialCmdAct *sca = (SerialCmdAct *) act;

	char rcv_chr;
	if (CharQueue_pop(sca->recvQueue->q, &rcv_chr)) {
		*(sca->cmd_ptr) = rcv_chr;
		sca->cmd_ptr += 1;
		if (sca->cmd_ptr == &sca->cmd[sizeof(sca->cmd)-1])
		{
			// Buffer full!
			rcv_chr = '\n';
		}
		if (rcv_chr == '\n')
		{
			*(sca->cmd_ptr) = '\0';
			sca->cmd_ptr = sca->cmd;
			(sca->cmd_cb)(sca->cmd);
		}
	}

	schedule_us(120, &sca->act);
}

#define FOSC 8000000// Clock Speed
#define BAUD 38400
#define MYUBRR FOSC/16/BAUD-1

void serial_echo_init(SerialCmdAct *sca, cmd_cb_f *cmd_cb)
{
	uart_init(RULOS_UART0, MYUBRR, TRUE);
	sca->act.func = serial_echo_update;
	sca->recvQueue = uart_recvq(RULOS_UART0);
	sca->cmd_ptr = sca->cmd;
	sca->cmd_cb = cmd_cb;

	schedule_us(1000, &sca->act);
}

//////////////////////////////////////////////////////////////////////////////

BlinkAct blink;
char reply_buf[80];

typedef struct {
	Activation act;
	SPIFlash spif;
	union {
		RingBuffer rb;
		char rb_space[50];
	};
	SPIBuffer spib;
} SDTest;

void sdtest_update(Activation *act)
{
	SDTest *sdt = (SDTest *) act;
	if (ring_remove_avail(&sdt->rb) < sizeof(sdt->rb_space))
	{
		// read hasn't completed. Wait a ms.
		schedule_us(1000, &sdt->act);
	}

	char *rp = reply_buf;
	*rp = ':'; rp+=1;
	while (ring_remove_avail(&sdt->rb))
	{
		char x = (char) ring_remove(&sdt->rb);
		*rp = x;
		rp += 1;
	}
	*rp = '\n'; rp+=1;
	*rp = '\0'; rp+=1;
	
	uart_send(RULOS_UART0, reply_buf, strlen(reply_buf), NULL, NULL);
	blink_once(&blink);
}

void sdtest_init(SDTest *sdt)
{
	init_ring_buffer(&sdt->rb, sizeof(sdt->rb_space));
	sdt->spib.start_addr = 0;
	sdt->spib.count = 0;
	sdt->spib.rb = &sdt->rb;
	init_spiflash(&sdt->spif);
	sdt->act.func = sdtest_update;
}

void sdtest_read(SDTest *sdt)
{
	sdt->spib.start_addr = 0;
	sdt->spib.count = sizeof(sdt->rb_space);
	spiflash_fill_buffer(&sdt->spif, &sdt->spib);
}

SDTest sdt;

void command_processor(const char *buf)
{
	if (strcmp(buf, "read\n")==0)
	{
		sdtest_read(&sdt);
	}
	else
	{
		strcpy(reply_buf, "error: \"");
		strcat(reply_buf, buf);
		strcat(reply_buf, "\"\n");
		uart_send(RULOS_UART0, reply_buf, strlen(reply_buf), NULL, NULL);
	}
	blink_once(&blink);
}

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);

	// start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	blink_init(&blink, FALSE);

	SerialCmdAct sca;
	serial_echo_init(&sca, command_processor);

	sdtest_init(&sdt);

	cpumon_main_loop();

#if 0
	uart_init(RULOS_UART0, 416);
	while (1)
	{
		char *uartSendBuf = "hi\n";
		uart_send(RULOS_UART0, uartSendBuf, 3, NULL, NULL);
	}
#endif

}

#if 0

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);

	// start clock with 10 msec resolution
	init_clock(10000, TIMER1);

#define FOSC 8000000// Clock Speed
#define BAUD 38400
#define MYUBRR FOSC/16/BAUD-1

#if 1
	// start the uart running at 500k baud (assumes 8mhz clock: fixme)
	uart_init(RULOS_UART0, MYUBRR, FALSE);
	UartQueue_t *recvQueue = uart_recvq(RULOS_UART0);
	
	gpio_make_output(GPIO_C5);

	int blink = 0;
	int i=0;
	while (1) {
		char uartSendBuf;

		uartSendBuf = 'A'+i;
		if (i==26)
		{
			uartSendBuf = '\r';
		} else if (i==27)
		{
			uartSendBuf = '\n';
		}

		i = i + 1;
		if (i>27) {
			i = 0;
			gpio_set_or_clr(GPIO_C5, (blink!=0));
			blink = !blink;
		}

/*
		r_bool rc = FALSE;
		while (!rc)
		{
			rc = uart_send(RULOS_UART0, &uartSendBuf, 1, NULL, NULL);
		}
*/
			
		if (CharQueue_pop(recvQueue->q, &uartSendBuf)) {
			uartSendBuf = toupper(uartSendBuf);
			uart_send(RULOS_UART0, &uartSendBuf, 1, NULL, NULL);
		}
	}
#endif

#if 0	// raw, no-interrupt send works, at 1200 baud, anyway.
	unsigned int ubrr = MYUBRR;
	/* Set baud rate */
	UBRRH = (unsigned char)(ubrr>>8);
	UBRRL = (unsigned char)ubrr;
	/* Enable receiver and transmitter */
	UCSRB = (1<<RXEN)|(1<<TXEN);
	/* Set frame format: 8data, 2stop bit */
	UCSRC = (1<<URSEL)|(1<<USBS)|(3<<UCSZ0);

	int i = 0;
	while (1)
	{
		/* Wait for empty transmit buffer */
		while ( !( UCSRA & (1<<UDRE)) )
			;
		/* Put data into buffer, sends the data */
		UDR = 'a'+i;
		i = i + 1;
		if (i>25) { i = 0; }
	}
#endif
}
#endif
