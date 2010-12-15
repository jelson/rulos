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

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "network.h"
#include "sim.h"
#include "audio_driver.h"
#include "audio_server.h"
#include "sdcard.h"

#define AUDIOTEST 0

#if AUDIOTEST
typedef struct {
	ActivationFunc func;
	AudioDriver *ad;
} AudioTest;

void at_update(AudioTest *at)
{
	schedule_us(1000000, (Activation*) at);
	ad_skip_to_clip(at->ad, 0, deadbeef_rand()%8, sound_silence);
}

void init_audio_test(AudioTest *at, AudioDriver *ad)
{
	at->func = (ActivationFunc) at_update;
	at->ad = ad;
	schedule_us(1, (Activation*)at);
}
#endif

//////////////////////////////////////////////////////////////////////////////
void audioled_init();
void audioled_set(r_bool red, r_bool yellow);
//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	UartQueue_t *recvQueue;
	char cmd[80];
	char *cmd_ptr;
	Activation *cmd_act;
} SerialCmdAct;

void serialcmd_update(Activation *act)
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
			(sca->cmd_act->func)(sca->cmd_act);
		}
	}

	schedule_us(120, &sca->act);
}

#define FOSC 8000000// Clock Speed
#define BAUD 38400
#define MYUBRR FOSC/16/BAUD-1

void serialcmd_init(SerialCmdAct *sca, Activation *cmd_act)
{
	uart_init(RULOS_UART0, MYUBRR, TRUE);
	sca->act.func = serialcmd_update;
	sca->recvQueue = uart_recvq(RULOS_UART0);
	sca->cmd_ptr = sca->cmd;
	sca->cmd_act = cmd_act;

	schedule_us(1000, &sca->act);
}

void waitbusyuart()
{
	while (uart_busy(RULOS_UART0)) { }
}

void serialcmd_sync_send(SerialCmdAct *act, char *buf, uint16_t buflen)
{
	uart_send(RULOS_UART0, buf, buflen, NULL, NULL);
	waitbusyuart();
}

#define SYNCDEBUG()	syncdebug(0, 'A', __LINE__)
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

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

#define AUDIO_LED_RED		GPIO_D2
#define AUDIO_LED_YELLOW	GPIO_D3

#ifndef SIM
#include "hardware.h"
#endif // SIM


void audioled_init()
{
#ifndef SIM
	gpio_make_output(AUDIO_LED_RED);
	gpio_make_output(AUDIO_LED_YELLOW);
#endif // SIM
}

void audioled_set(r_bool red, r_bool yellow)
{
#ifndef SIM
	gpio_set_or_clr(AUDIO_LED_RED, !red);
	gpio_set_or_clr(AUDIO_LED_YELLOW, !yellow);
#endif // SIM
}

//////////////////////////////////////////////////////////////////////////////

typedef enum
{
	ST_None = 0,
	ST_Initialize = 1,
	ST_Read = 2,
} STState;

typedef struct
{
	Activation act;
	SDCard sdc;
	r_bool initialized;
	STState issued;
	uint8_t buf[20];
	uint16_t buflen;
} STAct;

void _st_issue_init(STAct *sta)
{
	sta->issued = ST_Initialize;
	sdc_initialize(&sta->sdc, &sta->act);
}

void _st_issue_read(STAct *sta)
{
	sta->issued = ST_Read;
	sdc_read(&sta->sdc, 0, 0, sta->buf, sta->buflen, &sta->act);
}

void _st_update(Activation *act)
{
	STAct *sta = (STAct *) act;

	if (sta->issued == ST_Initialize)
	{
		if (sta->sdc.complete)
		{
			sta->initialized = TRUE;
			_st_issue_read(sta);
		}
		else
		{
			_st_issue_init(sta);
			// TODO infinite loop
		}
	}
	else if (sta->issued == ST_Read)
	{
		sta->issued = ST_None;
		// TODO activate parent.
	}
}

void st_init(STAct *sta)
{
	sta->act.func = _st_update;
	sdc_init(&sta->sdc);
	sta->initialized = FALSE;
	sta->issued = ST_None;
	sta->buflen = sizeof(sta->buf);
}

void st_read(STAct *sta)
{
	if (!sta->initialized)
	{
		_st_issue_init(sta);
	}
	else
	{
		_st_issue_read(sta);
	}
}

//////////////////////////////////////////////////////////////////////////////

void noop_update(Activation *act)
{
}
Activation noopact = { noop_update };

typedef struct {
	Activation act;
	uint8_t databuf[16];
} PrintMsgAct;

void _pma_update(Activation *act)
{
	PrintMsgAct *pma = (PrintMsgAct *) act;
	uart_send(RULOS_UART0, (char*) pma->databuf, sizeof(pma->databuf), NULL, NULL);
	waitbusyuart();
}

void pma_init(PrintMsgAct *pma)
{
	pma->act.func = _pma_update;
}

typedef struct {
	Activation act;
//	STAct sta;
	SDCard sdc;
	SerialCmdAct sca;
	PrintMsgAct pma;
} CmdProc;

int flip = 0;
void cmdproc_update(Activation *act)
{
	CmdProc *cp = (CmdProc *) act;
	char *buf = cp->sca.cmd;

	flip = !flip;
//	audioled_set(flip, 0);
	SYNCDEBUG();

	if (strcmp(buf, "init\n")==0)
	{
		sdc_initialize(&cp->sdc, &noopact);
	}
	else if (strncmp(buf, "read", 4)==0)
	{
		int halfoffset = buf[5]-'0';
		uint32_t block_offset = ((uint32_t)(halfoffset>>1))<<9;
		uint16_t skip = (halfoffset&1)<<8;
		syncdebug(0, 'h', halfoffset);
		syncdebug(0, 'b', block_offset);
		syncdebug(0, 's', skip);
		SYNCDEBUG();
		sdc_read(&cp->sdc, block_offset, skip, cp->pma.databuf, sizeof(cp->pma.databuf), &cp->pma.act);
	}
	else if (strcmp(buf, "led on\n")==0)
	{
		audioled_set(1, 1);
	}
	else if (strcmp(buf, "led off\n")==0)
	{
		audioled_set(0, 0);
	}
	else
	{
		char reply_buf[80];
		strcpy(reply_buf, "error: \"");
		strcat(reply_buf, buf);
		strcat(reply_buf, "\"\n");
		serialcmd_sync_send(&cp->sca, reply_buf, strlen(reply_buf));
	}
}

void cmdproc_init(CmdProc *cp)
{
	serialcmd_init(&cp->sca, &cp->act);
	pma_init(&cp->pma);
	SYNCDEBUG();
	cp->act.func = cmdproc_update;
//	st_init(&cp->sta);
	sdc_init(&cp->sdc);
	SYNCDEBUG();
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	CpumonAct cpumon;
	CmdProc cmdproc;
} MainContext;
MainContext mc;

int main()
{
	audioled_init();
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

	audioled_init();

	audioled_set(0, 0);
#if 0
	AudioDriver ad;
	init_audio_driver(&ad);

	Network network;
	init_network(&network);

	AudioServer as;
	init_audio_server(&as, &ad, &network, 0);

	ad_skip_to_clip(&ad, 0, sound_apollo_11_countdown, sound_apollo_11_countdown);
#if AUDIOTEST
	AudioTest at;
	init_audio_test(&at, &ad);
#endif
#endif

	cpumon_init(&mc.cpumon);	// includes slow calibration phase

	cmdproc_init(&mc.cmdproc);

	audioled_set(1, 0);
#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

