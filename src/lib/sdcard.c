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

#define R_SYNCDEBUG()	syncdebug(0, 'S', __LINE__)
//#define SYNCDEBUG()	R_SYNCDEBUG()
#define SYNCDEBUG()	{}
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

//#define SCHEDULE_SOON(act)	schedule_us(1, act)
#define SCHEDULE_SOON(act)	schedule_now(act)

void fill_value(uint8_t *dst, uint32_t src);

SEQDECL(SDCard, reset_card, 1_cmd0);
SEQDECL(SDCard, reset_card, 2_cmd1);
SEQDECL(SDCard, reset_card, 3_blocksize);
SEQDECL(SDCard, reset_card, 4_done);

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


void sdc_init(SDCard *sdc)
{
	sdc->act.func = NULL;	// varies depending on entry point.
	sdc->transaction_open = FALSE;
	spi_init(&sdc->spi);
}


//////////////////////////////////////////////////////////////////////////////
// Reset card transaction
//////////////////////////////////////////////////////////////////////////////

void _sdc_issue_spic_cmd(SDCard *sdc, uint8_t *cmd, uint8_t cmdlen, uint8_t cmd_expect_code)
{
	SYNCDEBUG();
	sdc->spic.cmd = cmd;
	sdc->spic.cmdlen = cmdlen;
	sdc->spic.cmd_expect_code = cmd_expect_code;
	sdc->spic.reply_buffer = NULL;
	sdc->spic.reply_buflen = 0;
	sdc->spic.done_act = &sdc->act;
	spi_start(&sdc->spi, &sdc->spic);
}


#define SPI_CMD(x)		((x) | 0x40)
#define SPI_CMD0_CRC	(0x95)
#define SPI_DUMMY_CRC	(0x01)
uint8_t _spicmd0[6] = { SPI_CMD(0), 0, 0, 0, 0, SPI_CMD0_CRC };
uint8_t _spicmd1[6] = { SPI_CMD(1), 0, 0, 0, 0, SPI_DUMMY_CRC };
uint8_t _spicmd16[6] = { SPI_CMD(16), 0, 0, 0x20, 0x00, SPI_DUMMY_CRC };

void sdc_reset_card(SDCard *sdc, Activation *done_act)
{
	SYNCDEBUG();
	sdc->transaction_open = TRUE;

	sdc->blocksize = 512;
	sdc->done_act = done_act;
	sdc->error = TRUE;

	hal_spi_set_fast(FALSE);
	{
		Activation *act = &sdc->act;
		SEQNEXT(SDCard, reset_card, 1_cmd0);
		bunchaclocks_start(&sdc->bunchaClocks, 16, act);
	}
}

#define SDCRESET_END(errorval) \
	SYNCDEBUG(); \
	sdc->error = errorval; \
	sdc->transaction_open = FALSE; \
	SCHEDULE_SOON(sdc->done_act); \
	return;

#define SDCRESET(i) \
SEQDEF(SDCard, reset_card, i, sdc) \
	if (sdc->spic.error == TRUE) \
	{ \
		SDCRESET_END(TRUE); \
	}

SEQDEF(SDCard, reset_card, 1_cmd0, sdc)
	_sdc_issue_spic_cmd(sdc, _spicmd0, 6, 1);
	SEQNEXT(SDCard, reset_card, 2_cmd1);
}

SDCRESET(2_cmd1)
	_sdc_issue_spic_cmd(sdc, _spicmd1, 6, 1);
	SEQNEXT(SDCard, reset_card, 3_blocksize);
}

SDCRESET(3_blocksize)
	fill_value(&_spicmd16[1], sdc->blocksize);
	_sdc_issue_spic_cmd(sdc, _spicmd16, 6, 0);
	SEQNEXT(SDCard, reset_card, 4_done);
}

SDCRESET(4_done)
	hal_spi_set_fast(TRUE);
	SDCRESET_END(FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// Read transaction
//////////////////////////////////////////////////////////////////////////////

SEQDECL(SDCard, read, 1_readmore);

void fill_value(uint8_t *dst, uint32_t src)
{
	dst[0] = (src>>(3*8)) & 0xff;
	dst[1] = (src>>(2*8)) & 0xff;
	dst[2] = (src>>(1*8)) & 0xff;
	dst[3] = (src>>(0*8)) & 0xff;
}

r_bool sdc_start_transaction(SDCard *sdc, uint32_t offset, uint8_t *buffer, uint16_t buflen, Activation *done_act)
{
	if (sdc->transaction_open)
	{
		SYNCDEBUG();
		return FALSE;
	}
	SYNCDEBUG();
	sdc->transaction_open = TRUE;
	sdc->error = FALSE;

	uint8_t *read_cmdseq = sdc->read_cmdseq;
	read_cmdseq[0] = SPI_CMD(17);
	fill_value(&read_cmdseq[1], offset);
	read_cmdseq[5] = SPI_DUMMY_CRC;

	sdc->spic.cmd = read_cmdseq;
	sdc->spic.cmdlen = sizeof(sdc->read_cmdseq);
	sdc->spic.cmd_expect_code = 0;

	sdc->spic.reply_buffer = buffer;
	sdc->spic.reply_buflen = buflen;
	sdc->done_act = done_act;
	{
		Activation *act = &sdc->act;
		sdc->spic.done_act = act;
		SEQNEXT(SDCard, read, 1_readmore);
		spi_start(&sdc->spi, &sdc->spic);
	}
	return TRUE;
}

SEQDEF(SDCard, read, 1_readmore, sdc)
	if (sdc->spic.error)
	{
		SYNCDEBUG();
		sdc->error = TRUE;
	}
	sdc->done_act->func(sdc->done_act);
}

r_bool sdc_is_error(SDCard *sdc)
{
	return sdc->error;
}

void sdc_continue_transaction(SDCard *sdc, uint8_t *buffer, uint16_t buflen, Activation *done_act)
{
	sdc->done_act = done_act;

	{
		Activation *act = &sdc->act;
		SEQNEXT(SDCard, read, 1_readmore);
		spi_resume_transfer(&sdc->spi, buffer, buflen);
	}
}

void sdc_end_transaction(SDCard *sdc, Activation *done_act)
{
	// could clock out CRC bytes, but why?
	// finish spi on "separate thread".
	sdc->spic.done_act = NULL;
	spi_finish(&sdc->spi);

	sdc->transaction_open = FALSE;
	if (done_act!=NULL)
	{
		schedule_now(done_act);
	}
	SYNCDEBUG();
}
