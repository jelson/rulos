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
//#include "spiflash.h"

#include "hardware.h"	// sorry. Blinky LED.

//////////////////////////////////////////////////////////////////////////////

#include "hardware.h"	// sorry. Blinky LED.

typedef struct {
	int state;
} BlinkAct;

void blink_once(BlinkAct *ba)
{
	ba->state = (ba->state==0);
	gpio_set_or_clr(GPIO_C5, (ba->state!=0));
}

void blink_update(BlinkAct *ba)
{
	blink_once(ba);
	schedule_us(100000, (ActivationFuncPtr) blink_update, ba);
}

void blink_init(BlinkAct *ba, r_bool run)
{
	gpio_make_output(GPIO_C5);
	if (run)
	{
		schedule_us(50000, (ActivationFuncPtr) blink_update, ba);
	}
}

//////////////////////////////////////////////////////////////////////////////

void debug_result(char *msg, uint16_t state)
{
	uint8_t i;
	gpio_set_or_clr(GPIO_C3, 1);	// word align
	for (i=0; i<8; i++)
	{
		gpio_set_or_clr(GPIO_C4, (state>>(7-i)) & 1);	// data
		gpio_set_or_clr(GPIO_C2, 1); // clock
		gpio_set_or_clr(GPIO_C2, 0); // clock
	}
	gpio_set_or_clr(GPIO_C4, 0); // clear data
	gpio_set_or_clr(GPIO_C3, 0); // clear word align

	char buf[50];
	strcpy(buf, msg);
	strcat(buf, " ds: ");
	debug_itoha(buf+strlen(buf), state);
	strcat(buf, ".\n");
	uart_send(RULOS_UART0, buf, strlen(buf), NULL, NULL);
	while (uart_busy(RULOS_UART0))
	{ }
}

void debug_state(uint16_t state)
{
	debug_result("state", state);
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

void serial_echo_update(SerialCmdAct *sca)
{
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

	schedule_us(120, (ActivationFuncPtr) serial_echo_update, sca);
}

#define FOSC 8000000// Clock Speed
#define BAUD 38400
#define MYUBRR FOSC/16/BAUD-1

void serial_echo_init(SerialCmdAct *sca, cmd_cb_f *cmd_cb)
{
	uart_init(RULOS_UART0, MYUBRR, TRUE);
	sca->recvQueue = uart_recvq(RULOS_UART0);
	sca->cmd_ptr = sca->cmd;
	sca->cmd_cb = cmd_cb;

	schedule_us(1000, (ActivationFuncPtr) serial_echo_update, sca);
}

//////////////////////////////////////////////////////////////////////////////

char reply_buf[80];
BlinkAct blink;
#if 0

typedef struct {
	Activation act;
	SPIFlash spif;
	union {
		RingBuffer rb;
		char rb_space[50];
	};
	SPIBuffer spib;
} SDTest;

void sdtest_update(SDTest *sdt)
{
	if (ring_remove_avail(&sdt->rb) < sizeof(sdt->rb_space))
	{
		// read hasn't completed. Wait a ms.
		schedule_us(1000, (ActivationFuncPtr) sdtest_update, sdt);
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
}

void sdtest_read(SDTest *sdt)
{
	sdt->spib.start_addr = 0;
	sdt->spib.count = sizeof(sdt->rb_space);
	spiflash_fill_buffer(&sdt->spif, &sdt->spib);
}

SDTest sdt;
#endif

void spi_test(void);

void command_processor(const char *buf)
{
	if (strcmp(buf, "read\n")==0)
	{
		//sdtest_read(&sdt);
		spi_test();
	}
	else if (strncmp(buf, "spcr", 4)==0)
	{
		uint8_t clockinfo = (buf[5]&0x3);
		uint8_t spcr = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (0<<SPR0);
		spcr |= (clockinfo<<2);
		SPCR = spcr;
		debug_result("spcr", spcr);
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

#define SPI_SS	GPIO_B2

void delay()
{
	volatile int x;
	uint32_t i;
	for (i=0; i<100000; i++)
	{
		x = x + 1;
	}
}

void xmit_spi(uint8_t dat)
{
	SPDR = (dat);
	while ((SPSR & (_BV(SPIF))) == 0) { }
	loop_until_bit_is_set(SPSR, SPIF);
}

uint8_t rcvr_spi(void)
{
	SPDR = 0xff;
	while ((SPSR & (_BV(SPIF))) == 0) { }
	return SPDR;
}

int wait_ready(void)
{
// TODO use real timeouts.
	rcvr_spi();
	int i;
	for (i=0; i<1000; i++)
	{
		if (rcvr_spi() == 0xff) return 1;
	}
	return 0;
}

void deselect(void)
{
	gpio_set_or_clr(SPI_SS, 1);
	//rcvr_spi();
}

r_bool select(void)
{
	gpio_set_or_clr(SPI_SS, 0);
	if (!wait_ready()) {
	    deselect();
		return 0;
	}
	return 1;
}

/* Definitions for MMC/SDC command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* Card type flags (CardType) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */



uint8_t start_cmd(
	uint8_t cmd,
	uint32_t arg)
{
	uint8_t crc, result;

	gpio_set_or_clr(SPI_SS, 0);

	debug_result("cmd index", cmd);
//	debug_state(0x31);
	xmit_spi(0x40 | cmd);	// Start + command index
//	debug_state(0x32);
	xmit_spi((uint8_t)(arg >> 24));
	xmit_spi((uint8_t)(arg >> 16));
	xmit_spi((uint8_t)(arg >>  8));
	xmit_spi((uint8_t)(arg >>  0));
	crc = 0x01;	// dummy CRC + stop
	if (cmd == CMD0) { crc = 0x95; }	// valid CRC for CMD0(0)
	if (cmd == CMD8) { crc = 0x87; }	// valid CRC for CMD8(0x1AA)
	xmit_spi(crc);

	if (cmd == CMD12) rcvr_spi();	// skip a stuff byte when stop reading

	int n = 10;
	do
	{
		result = rcvr_spi();
	} while ((result & 0x80) && --n);
	debug_result("cmd result", result);

	return result;
}

void end_cmd()
{
	gpio_set_or_clr(SPI_SS, 1);

	// give sd card 8 cycles to finish its work
	// v1.9 section 5.1.8
	xmit_spi(0xff);
}

uint8_t send_cmd(
	uint8_t cmd,
	uint32_t arg)
{
	uint8_t result = start_cmd(cmd, arg);
	end_cmd();
	return result;
}

int rcvr_datablock(uint8_t *buf, uint16_t buflen)
{
	uint8_t token;

	debug_state(0x50);
	int maxloops = 8;
	do
	{
		token = rcvr_spi();
		debug_result("db", token);
	} while ((token != 0xfe) && (maxloops-->0));

	if (token != 0xfe) { return 0; }

	debug_state(0x51);
	do {
		debug_state(0x52);
		char dummy;
		dummy = rcvr_spi();
		dummy = rcvr_spi();
		dummy = rcvr_spi();
		dummy = rcvr_spi();
		debug_result("read_byte", dummy);
/*
		*(buf++) = rcvr_spi();
		*(buf++) = rcvr_spi();
		*(buf++) = rcvr_spi();
		*(buf++) = rcvr_spi();
*/
	} while (buflen -= 4);
	debug_state(0x53);
	// discard crc
	rcvr_spi();
	rcvr_spi();
	debug_state(0x54);
	return 1;
}

void disk_read(
	uint8_t *buf,
	uint16_t buflen,
	uint32_t byte_offset)
{
	// hoping !(CardType & CT_BLOCK)

	debug_state(0x40);
	send_cmd(CMD17, byte_offset);

	debug_state(0x42);

	debug_result("bufptr", (uint16_t) buf);
	debug_result("buflen", buflen);

	gpio_set_or_clr(SPI_SS, 0);
	rcvr_datablock(buf, buflen);
	gpio_set_or_clr(SPI_SS, 1);

	debug_state(0x44);
	end_cmd();
}

void drain()
{
	int i;
	for (i=0; i<10; i++)
	{
		rcvr_spi();
	}
}

uint8_t disk_initialize ()
{
	uint8_t n;
	//, cmd, ocr[4];
	uint8_t ty = 0;

	debug_state(4);

	deselect();
	for (n = 16; n; n--) rcvr_spi();	/* 96 dummy clocks */

	debug_state(0x10);

	uint8_t result;
	result = send_cmd(CMD0, 0);
	//drain();
	if (result != 1)
	{
		debug_state(0x11);
		debug_result("CMD0", result);
		goto fail;
	}
	
	/*
	debug_state(0x12);
	result = send_cmd(CMD8, 0x000001aa);
	debug_state(0x13);
	debug_state(result);
	*/

	debug_state(0x14);
	result = send_cmd(CMD1, 0x00000000);
	if (result != 1)
	{
		debug_state(0x15);
		debug_result("CMD1", result);
		goto fail;
	}

/*
	debug_state(0x12);
	result = send_cmd(CMD8, 0x000001aa);
	debug_state(0x13);
	debug_state(result);
	*/

	debug_state(0x16);
	result = send_cmd(CMD16, 0x00000200);
	debug_state(0x17);
	debug_result("CMD16", result);

	return 1;

fail:
	debug_state(0x30);
	deselect();
	return ty;
}


void raw_spi_test()
{
#if 0
	gpio_set_or_clr(GPIO_C5, 0);
	delay();
	gpio_set_or_clr(GPIO_C5, 1);
	delay();
#endif

	//gpio_set_or_clr(SPI_SS, 0);
	uint8_t readbuf[8];
	disk_read(readbuf, sizeof(readbuf), 0);
	//gpio_set_or_clr(SPI_SS, 1);
}

void spi_test(void)
{
	// program start trigger
	//gpio_set_or_clr(GPIO_C5, 0);
	//gpio_set_or_clr(GPIO_C5, 1);

	blink_once(&blink);
	debug_state(1);

	while (1)
	{
		debug_state(2);
		if (disk_initialize()!=0)
		{
			break;
		}
	}

	debug_state(3);
	raw_spi_test();
	debug_state(6);
}

int main()
{
	hal_init();

	// start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	blink_init(&blink, FALSE);

#define AUDIO_LED_RED		GPIO_D2
#define AUDIO_LED_YELLOW	GPIO_D3
	gpio_make_output(AUDIO_LED_RED);
	gpio_make_output(AUDIO_LED_YELLOW);
	gpio_set_or_clr(AUDIO_LED_RED, 0);
	gpio_set_or_clr(AUDIO_LED_YELLOW, 1);

	gpio_make_output(GPIO_C5);
	gpio_make_output(GPIO_C4);
	gpio_make_output(GPIO_C3);
	gpio_make_output(GPIO_C2);
//	HALSPIIfc spi;
//	hal_init_spi(&spi);


	SerialCmdAct sca;
	serial_echo_init(&sca, command_processor);

#if 0
	// voltage test
	gpio_make_output(GPIO_B2);
	gpio_make_output(GPIO_B3);
	gpio_make_output(GPIO_B5);
	gpio_set_or_clr(GPIO_B2, 1);
	gpio_set_or_clr(GPIO_B3, 1);
	gpio_set_or_clr(GPIO_B5, 1);

	while (1)
	{
		debug_result("hello!", 7);
		delay();
		gpio_set_or_clr(AUDIO_LED_YELLOW, 0);
		debug_result("goodbye!", 7);
		delay();
		gpio_set_or_clr(AUDIO_LED_YELLOW, 1);
	}
#endif

	//sdtest_init(&sdt);

	cpumon_main_loop();
}

