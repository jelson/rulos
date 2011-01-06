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

#define SEQDECL(T,f,i)	\
void T##_##f##_##i(Activation *act)

#define SEQDEF(T,f,i,t)	\
SEQDECL(T,f,i) \
{ \
	T *t = ((T*) act); \
	SYNCDEBUG();

#define SEQNEXT(T,f,i) \
	act->func = &T##_##f##_##i; \
	//syncdebug(3, 'a', (int) (act->func)<<1)


#define R_SYNCDEBUG()	syncdebug(0, 'S', __LINE__)
//#define SYNCDEBUG()	R_SYNCDEBUG()
#define SYNCDEBUG()	{}
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

//#define SCHEDULE_SOON(act)	schedule_us(1, act)
#define SCHEDULE_SOON(act)	schedule_now(act)

// TODO on logic analyzer:
// How slow are we going due to off-interrupt rescheduling?

// Why's there such a noticeably-long delay between last message
// and the PrintMsgAct firing?
// That one's easy: we're waiting for the HANDLER_DELAY of 1 clock
// (1ms) * 512 bytes to read = 512ms.

void fill_value(uint8_t *dst, uint32_t src);

//////////////////////////////////////////////////////////////////////////////

void _bunchaclocks_handler(HALSPIHandler *h, uint8_t data)
{
	BunchaClocks *bc = (BunchaClocks *) h;
	if (bc->numclocks_bytes == 0)
	{
		schedule_us(1000, bc->done_act);
	}
	else
	{
		bc->numclocks_bytes--;
		hal_spi_send(0xff);
	}
}

void bunchaclocks_start(BunchaClocks *bc, int numclocks_bytes, Activation *done_act)
{
	SYNCDEBUG();
	bc->handler.func = _bunchaclocks_handler;
	bc->numclocks_bytes = numclocks_bytes-1;
	bc->done_act = done_act;
	hal_spi_set_handler(&bc->handler);

	hal_spi_send(0xff);
}

//////////////////////////////////////////////////////////////////////////////

void _spi_spif_handler(HALSPIHandler *h, uint8_t data);
SEQDECL(SPIAct, update, 0_send_cmd);
SEQDECL(SPIAct, update, 1_wait_cmd_ack);
SEQDECL(SPIAct, update, 2_wait_reply);
SEQDECL(SPIAct, update, 3_read_reply);

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
	spic->complete = FALSE;
	spi->spic = spic;

	spi->cmd_i = 0;
	spi->cmd_wait = SPI_CMD_WAIT_LIMIT;
	spi->reply_i = 0;
	spi->reply_wait = SPI_REPLY_WAIT_LIMIT;
	spi->cmd_acknowledged = FALSE;
	spi->reply_started = FALSE;
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

#define SPIFFASTREAD 1
#if SPIFFASTREAD
void _spi_spif_fastread_handler(HALSPIHandler *h, uint8_t data)
{
	SPI *spi = (SPI*) h;
	SPICmd *spic = spi->spic;

	uint16_t reply_i = spi->reply_i;
	if (reply_i < spic->replylen)
	{
		spic->replydata[reply_i] = data;
		hal_spi_send(0xff);
	}
	else
	{
		// reads once too many, but avoids a compare in main loop
		spi->handler.func = _spi_spif_handler;
		_spi_spif_handler(h, data);
	}
	reply_i++;
	spi->reply_i = reply_i;
}
#endif // SPIFFASTREAD

#define READABLE(c)	(((c)>=' '&&(c)<127)?(c):'_')

#define SPIFINISH(result) \
{ \
	spic->complete = result; \
	hal_spi_select_slave(FALSE); \
	schedule_now(spic->done_act); \
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
		SPIFINISH(FALSE);
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
		SYNCDEBUG();
		syncdebug(3, 'g', spi->data);
		syncdebug(3, 'e', spic->cmd_expect_code);
		// expect_code mismatch
		SPIFINISH(FALSE);
	}
}

SPIDEF(2_wait_reply)
	if (spic->replylen==0) // not expecting any data
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
	else if (spi->data == 0xfe) // signals start of reply data
	{
		SYNCDEBUG();
		SEQNEXT(SPIAct, update, 3_read_reply);
#if SPIFFASTREAD
		spi->handler.func = _spi_spif_fastread_handler;
#endif // SPIFFASTREAD
		hal_spi_send(0xff);
	}
	else if (spi->reply_wait==0) // timeout
	{
		SYNCDEBUG();
		SPIFINISH(FALSE);
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
		SYNCDEBUG();
		SPIFINISH(FALSE);
	}
}

SPIDEF(3_read_reply)
	spic->replydata[spi->reply_i] = spi->data;
	spi->reply_i++;
	if (spi->reply_i >= spic->replylen) // reply complete
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
	else
	{
		hal_spi_send(0xff);
	}
}

//////////////////////////////////////////////////////////////////////////////

void sdc_init(SDCard *sdc)
{
	sdc->act.func = NULL;	// varies depending on entry point.
	sdc->busy = FALSE;
	spi_init(&sdc->spi);
}

void _sdc_issue_spic_cmd(SDCard *sdc, uint8_t *cmd, uint8_t cmdlen, uint8_t cmd_expect_code)
{
	SYNCDEBUG();
	sdc->spic.cmd = cmd;
	sdc->spic.cmdlen = cmdlen;
	sdc->spic.cmd_expect_code = cmd_expect_code;
	sdc->spic.replydata = NULL;
	sdc->spic.replylen = 0;
	sdc->spic.done_act = &sdc->act;
	spi_start(&sdc->spi, &sdc->spic);
}

#define SPI_CMD(x)		((x) | 0x40)
#define SPI_CMD0_CRC	(0x95)
#define SPI_DUMMY_CRC	(0x01)
uint8_t _spicmd0[6] = { SPI_CMD(0), 0, 0, 0, 0, SPI_CMD0_CRC };
uint8_t _spicmd1[6] = { SPI_CMD(1), 0, 0, 0, 0, SPI_DUMMY_CRC };
uint8_t _spicmd16[6] = { SPI_CMD(16), 0, 0, 0x20, 0x00, SPI_DUMMY_CRC };

void _sdc_init_update(Activation *act)
{
	SYNCDEBUG();
	SDCard *sdc = (SDCard *) act;

	if (sdc->spic.complete == FALSE)
	{
		SYNCDEBUG();
		sdc->complete = FALSE;
		SCHEDULE_SOON(sdc->done_act);
		return;
	}

	switch (sdc->init_state)
	{
	case 0:
		SYNCDEBUG();
		sdc->spic.complete = TRUE;
			// NB yeah that was gross. We check this flag after each
			// continuation, and bunchaclocks always succeeds, so we
			// cheat by just setting it TRUE.
		bunchaclocks_start(&sdc->bunchaClocks, 16, &sdc->act);
		break;
	case 1:
		SYNCDEBUG();
		_sdc_issue_spic_cmd(sdc, _spicmd0, 6, 1);
		break;
	case 2:
		SYNCDEBUG();
		_sdc_issue_spic_cmd(sdc, _spicmd1, 6, 1);
		break;
	case 3:
		SYNCDEBUG();
		fill_value(&_spicmd16[1], sdc->blocksize);
		_sdc_issue_spic_cmd(sdc, _spicmd16, 6, 0);
		break;
	case 4:
		SYNCDEBUG();
		sdc->busy = FALSE;
		sdc->complete = TRUE;
		hal_spi_set_fast(TRUE);
		SCHEDULE_SOON(sdc->done_act);
		return;
	}
	// next time, do the next thing.
	sdc->init_state += 1;
}

void sdc_initialize(SDCard *sdc, Activation *done_act)
{
	sdc->busy = TRUE;
	sdc->act.func = _sdc_init_update;
	sdc->blocksize = 512;
	sdc->done_act = done_act;
	sdc->init_state = 0;
	sdc->complete = FALSE;

	hal_spi_set_fast(FALSE);
	sdc->spic.complete = TRUE;	// ensure first call doesn't quit
	_sdc_init_update(&sdc->act);
}

void _sdc_read_update(Activation *act)
{
	SDCard *sdc = (SDCard *) act;

	SYNCDEBUG();
	sdc->complete = sdc->spic.complete;
	sdc->busy = FALSE;
	if (sdc->done_act!=NULL)
	{
		SCHEDULE_SOON(sdc->done_act);
	}
}

void fill_value(uint8_t *dst, uint32_t src)
{
	dst[0] = (src>>(3*8)) & 0xff;
	dst[1] = (src>>(2*8)) & 0xff;
	dst[2] = (src>>(1*8)) & 0xff;
	dst[3] = (src>>(0*8)) & 0xff;
}

r_bool sdc_read(SDCard *sdc, uint32_t offset, Activation *done_act)
{
	SYNCDEBUG();
	if (sdc->busy)
	{
		// TODO indicate that this failed. Right now,
		// this path should only occur when being called in the blind
		// by audio_streamer, who will just call again later.
		SYNCDEBUG();
		return FALSE;
	}
	SYNCDEBUG();
	sdc->busy = TRUE;
	sdc->act.func = _sdc_read_update;
	sdc->done_act = done_act;
	sdc->complete = FALSE;

	uint8_t *read_cmdseq = sdc->read_cmdseq;
	read_cmdseq[0] = SPI_CMD(17);
	fill_value(&read_cmdseq[1], offset);
	read_cmdseq[5] = SPI_DUMMY_CRC;

#if 0
	syncdebug(4, 'c', read_cmdseq[0]);
	syncdebug(4, 'c', read_cmdseq[1]);
	syncdebug(4, 'c', read_cmdseq[2]);
	syncdebug(4, 'c', read_cmdseq[3]);
	syncdebug(4, 'c', read_cmdseq[4]);
	syncdebug(4, 'c', read_cmdseq[5]);
#endif

	sdc->spic.cmd = read_cmdseq;
	sdc->spic.cmdlen = sizeof(sdc->read_cmdseq);
	sdc->spic.cmd_expect_code = 0;
	sdc->spic.replydata = sdc->blockbuffer;
	sdc->spic.replylen = sizeof(sdc->blockbuffer);
	sdc->spic.done_act = &sdc->act;
	spi_start(&sdc->spi, &sdc->spic);
	return TRUE;
}

