#include "sdcard.h"

#define SYNCDEBUG()	syncdebug(0, 'S', __LINE__)
//#define SYNCDEBUG()	/*syncdebug(0, 'S', __LINE__)*/
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

//////////////////////////////////////////////////////////////////////////////

struct {
	Activation act;
	BunchaClocks *bc;
} bc_report;

void bc_report_func(Activation *act)
{
	SYNCDEBUG();
}

void _bunchaclocks_handler(HALSPIHandler *h, uint8_t data)
{
	bc_report.act.func = bc_report_func;
	schedule_us(1000, &bc_report.act);

	BunchaClocks *bc = (BunchaClocks *) h;
	if (bc->numclocks_bytes == 0)
	{
		extern void audioled_set(r_bool red, r_bool yellow);
		audioled_set(0, 1);
	
		schedule_us(1000, bc->done_act);

		extern void audioled_set(r_bool red, r_bool yellow);
		audioled_set(1, 0);
	}
	else
	{
		extern void audioled_set(r_bool red, r_bool yellow);
		audioled_set(0, 0);

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
	{
		extern HALSPIHandler *g_spi_handler;
		extern void waitbusyuart();
		char buf[16];
		debug_itoha(buf, (int) g_spi_handler);
		uart_send(RULOS_UART0, (char*) buf, strlen(buf), NULL, NULL);
		waitbusyuart();
	}

	hal_spi_send(0xff);
}

//////////////////////////////////////////////////////////////////////////////

void _spi_spif_handler(HALSPIHandler *h, uint8_t data);
void _spi_update(Activation *act);

void spi_init(SPI *spi)
{
	SYNCDEBUG();
	spi->handler.func = _spi_spif_handler;
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
	spi->spiact.act.func = _spi_update;
	spi->spiact.spi = spi;

	hal_spi_select_slave(TRUE);
	hal_spi_set_handler(&spi->handler);
	// call handler in activation context to issue initial send.
	_spi_spif_handler(&spi->handler, 0xff);
}

void _spi_spif_handler(HALSPIHandler *h, uint8_t data)
{
	SPI *spi = (SPI*) h;
	spi->data = data;
	schedule_us(1, &spi->spiact.act);
}

void _spi_update(Activation *act)
{
	SPI *spi = ((SPIAct*) act)->spi;
#define SPIFINISH(state)	{ \
	spic->complete = state; \
	hal_spi_select_slave(FALSE); \
	schedule_us(1, spic->done_act); \
	}
	syncdebug(3, spi->data, spi->data);

	SPICmd *spic = spi->spic;
	if (spi->cmd_i < spic->cmdlen)
	{
		SYNCDEBUG();
		uint8_t out_val = spic->cmd[spi->cmd_i];
		hal_spi_send(out_val);
		syncdebug(2, 'o', out_val);
		spi->cmd_i++;
	}
	else if (!spi->cmd_acknowledged)
	{
		if (spi->cmd_wait==0)
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
		else if (spi->data==spic->cmd_expect_code)
		{
			SYNCDEBUG();
			spi->cmd_acknowledged = TRUE;
			hal_spi_select_slave(FALSE);
			hal_spi_select_slave(TRUE);
			// recurse to start looking for the data block
			// Pretend we just heard "keep asking".
			_spi_spif_handler(&spi->handler, 0xff);
		}
		else
		{
			SYNCDEBUG();
			syncdebug(3, 'g', spi->data);
			syncdebug(3, 'e', spic->cmd_expect_code);
			// expect_code mismatch
			SPIFINISH(FALSE);
		}
	}
	else if (spic->replylen==0)
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
	else if (!spi->reply_started)
	{
		if (spi->reply_wait==0)
		{
			SYNCDEBUG();
			SPIFINISH(FALSE);
		}
		else if (spi->data != 0xfe)
		{
			SYNCDEBUG();
			// keep asking
			hal_spi_send(0xff);
			spi->reply_wait -= 1;
		}
		else
		{
			SYNCDEBUG();
			spi->reply_started = TRUE;
			// ask for first byte
			hal_spi_send(0xff);
		}
	}
	else if (spi->reply_i < spic->replylen)
	{
		spic->replydata[spi->reply_i] = spi->data;
		spi->reply_i++;
		hal_spi_send(0xff);
	}
	else
	{
		SYNCDEBUG();
		SPIFINISH(TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////////

void sdc_init(SDCard *sdc)
{
	sdc->act.func = NULL;	// varies depending on entry point.
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
	extern void audioled_set(r_bool red, r_bool yellow);
	audioled_set(0, 0);

	SYNCDEBUG();
	SDCard *sdc = (SDCard *) act;

	if (sdc->spic.complete == FALSE)
	{
		SYNCDEBUG();
		sdc->complete = FALSE;
		schedule_us(1, sdc->done_act);
		return;
	}

	switch (sdc->init_state)
	{
	case 0:
		SYNCDEBUG();
		extern void audioled_set(r_bool red, r_bool yellow);
		audioled_set(0, 0);
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
		_sdc_issue_spic_cmd(sdc, _spicmd16, 6, 0);
		break;
	case 4:
		SYNCDEBUG();
		sdc->complete = TRUE;
		schedule_us(1, sdc->done_act);
		return;
	}
	// next time, do the next thing.
	sdc->init_state += 1;
}

void sdc_initialize(SDCard *sdc, Activation *done_act)
{
	sdc->act.func = _sdc_init_update;
	sdc->done_act = done_act;
	sdc->init_state = 0;
	sdc->complete = FALSE;

	sdc->spic.complete = TRUE;	// ensure first call doesn't quit
	_sdc_init_update(&sdc->act);
}

void _sdc_read_update(Activation *act)
{
	SDCard *sdc = (SDCard *) act;

	SYNCDEBUG();
	sdc->complete = sdc->spic.complete;
	schedule_us(1, sdc->done_act);
}

void sdc_read(SDCard *sdc, uint32_t offset, uint8_t *buf, uint16_t buflen, Activation *done_act)
{
	SYNCDEBUG();
	sdc->act.func = _sdc_read_update;
	sdc->done_act = done_act;
	sdc->complete = FALSE;

	uint8_t cmdseq[6];
	cmdseq[0] = SPI_CMD(17);
	cmdseq[1] = (offset>>(3*8)) & 0xff;
	cmdseq[2] = (offset>>(2*8)) & 0xff;
	cmdseq[3] = (offset>>(1*8)) & 0xff;
	cmdseq[4] = (offset>>(0*8)) & 0xff;
	cmdseq[5] = SPI_DUMMY_CRC;

	sdc->spic.cmd = cmdseq;
	sdc->spic.cmdlen = sizeof(cmdseq);
	sdc->spic.cmd_expect_code = 0;
	sdc->spic.replydata = buf;
	sdc->spic.replylen = buflen;
		// TODO ugh, it's caller's job to absorb CRC.
		// Or maybe we could fail to clock it out it? Tee hee. Yeah, that works.
	sdc->spic.done_act = &sdc->act;
	spi_start(&sdc->spi, &sdc->spic);
}

//////////////////////////////////////////////////////////////////////////////

void sd_init(SDCard *sdc)
{
}

