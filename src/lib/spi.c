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

#include "sdcard.h"
#include "seqmacro.h"

#define R_SYNCDEBUG()	syncdebug(0, 'I', __LINE__)
//#define SYNCDEBUG()	R_SYNCDEBUG()
#define SYNCDEBUG()	{}
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

// TODO on logic analyzer:
// How slow are we going due to off-interrupt rescheduling?

// Why's there such a noticeably-long delay between last message
// and the PrintMsgAct firing?
// That one's easy: we're waiting for the HANDLER_DELAY of 1 clock
// (1ms) * 512 bytes to read = 512ms.

//////////////////////////////////////////////////////////////////////////////

void _spi_spif_handler(HALSPIHandler *h, uint8_t data);
SEQDECL(SPIAct, update, 0_send_cmd);
SEQDECL(SPIAct, update, 1_wait_cmd_ack);
SEQDECL(SPIAct, update, 2_wait_reply);
SEQDECL(SPIAct, update, 3_wait_buffer);


void _spi_fastread(Activation *act);

void spi_init(SPI *spi)
{
	SYNCDEBUG();
	spi->spic = NULL;
	hal_init_spi();
}

#define SPI_CMD_WAIT_LIMIT 500
#define SPI_REPLY_WAIT_LIMIT 500

void spi_start(SPI *spi, SPICmd *spic)
{
	SYNCDEBUG();
	spic->error = FALSE;
	spi->spic = spic;

	spi->cmd_i = 0;
	spi->cmd_wait = SPI_CMD_WAIT_LIMIT;
	spi->reply_wait = SPI_REPLY_WAIT_LIMIT;
	{
		Activation *act = &spi->spiact.act;
		SEQNEXT(SPIAct, update, 0_send_cmd);
	}
	spi->spiact.spi = spi;

	hal_spi_select_slave(TRUE);
	spi->handler.func = _spi_spif_handler;
	hal_spi_set_handler(&spi->handler);
	// call handler in activation context to issue initial send.
	_spi_spif_handler(&spi->handler, 0xff);
}

void _spi_spif_handler(HALSPIHandler *h, uint8_t data)
{
	SPI *spi = (SPI*) h;
	spi->data = data;
	schedule_now(&spi->spiact.act);
}

void _spi_spif_fastread_handler(HALSPIHandler *h, uint8_t data)
{
	SPI *spi = (SPI*) h;
	SPICmd *spic = spi->spic;

	*(spic->reply_buffer) = data;
	spic->reply_buffer+=1;
	spic->reply_buflen-=1;
	if ((spic->reply_buflen)>0)
	{
		// ready for next byte in fast handler
		hal_spi_send(0xff);
	}
	else
	{
		// nowhere to put next byte; go ask for more buffer
		_spi_spif_handler(h, data);
	}
}

#define READABLE(c)	(((c)>=' '&&(c)<127)?(c):'_')

#define SPIFINISH(result) \
{ \
	spic->error = result; \
	hal_spi_select_slave(FALSE); \
	if (spic->done_act!=NULL) { \
		schedule_now(spic->done_act); \
	} \
}

#define SPIDEF(i) \
SEQDEF(SPIAct, update, i, spiact) \
	SPI *spi = spiact->spi; \
	SPICmd *spic = spi->spic;

SPIDEF(0_send_cmd)
	uint8_t out_val = spic->cmd[spi->cmd_i];
	hal_spi_send(out_val);
//	syncdebug(4, 'o', out_val);
	spi->cmd_i++;
	if (spi->cmd_i >= spic->cmdlen)
	{
		SEQNEXT(SPIAct, update, 1_wait_cmd_ack);
	}
}

SPIDEF(1_wait_cmd_ack)
//	syncdebug(4, 'd', spi->data);
	if (spi->data == spic->cmd_expect_code)
	{
		SEQNEXT(SPIAct, update, 2_wait_reply);
		// NB we don't drop/reopen CS line, per
		// SecureDigitalCard_1.9.doc page 31, page 3-6, sec 3.3.
		// recurse to start looking for the data block
		// Pretend we just heard "keep asking".
		_spi_spif_handler(&spi->handler, 0xff);
	}
	else if (spi->cmd_wait==0)
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
	else if (spi->data & 0x80)
	{
		SYNCDEBUG();
		hal_spi_send(0xff);
		spi->cmd_wait -= 1;
	}
	else
	{
		R_SYNCDEBUG();
		syncdebug(3, 'g', spi->data);
		syncdebug(3, 'e', spic->cmd_expect_code);
		// expect_code mismatch
		SPIFINISH(TRUE);
	}
}

SPIDEF(2_wait_reply)
	if (spic->reply_buflen==0) // not expecting any data
	{
		SYNCDEBUG();
		SPIFINISH(FALSE);
	}
	else if (spi->data == 0xfe) // signals start of reply data
	{
		SYNCDEBUG();
		// assumes we have a buffer ready right now,
		// so this first byte has somewhere to go.
		SEQNEXT(SPIAct, update, 3_wait_buffer);
		spi->handler.func = _spi_spif_fastread_handler;
		hal_spi_send(0xff);
	}
	else if (spi->reply_wait==0) // timeout
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
	else if (spi->data & 0x80) // still waiting
	{
		SYNCDEBUG();
		// keep asking
		hal_spi_send(0xff);
		spi->reply_wait -= 1;
	}
	else // invalid reply code
	{
		syncdebug(4, 'v', spi->data);
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
}

SPIDEF(3_wait_buffer)
	// prompt caller for more buffer space
	spic->done_act->func(spic->done_act);
}

void spi_resume_transfer(SPI *spi, uint8_t *reply_buffer, uint16_t reply_buflen)
{
	spi->spic->reply_buffer = reply_buffer;
	spi->spic->reply_buflen = reply_buflen;
	// poke at SPI to restart flow of bytes into fast handler
	hal_spi_send(0xff);
	// NB act still points at 3_wait_buffer, so when handler runs
	// out of space, we'll get dropped back into 3_wait_buffer
}

void spi_finish(SPI *spi)
{
	SPICmd *spic = spi->spic;
	SYNCDEBUG();
	SPIFINISH(FALSE);
}

