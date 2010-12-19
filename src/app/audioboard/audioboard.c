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
#include "audio_streamer.h"
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

void syncdebug32(uint8_t spaces, char f, uint32_t line)
{
	syncdebug(spaces, f, line>>16);
	syncdebug(spaces, '~', line&0x0ffff);
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


int flip = 0;

typedef struct {
	uint32_t start_offset;
	uint32_t end_offset;
	uint16_t block_offset;
	uint16_t padding;
} AuIndexRec;

typedef struct {
	Activation act;
	AudioStreamer *as;
	uint8_t au_index;
} PlayAct;

void _play_proceed(Activation *act);

void play(PlayAct *pa, AudioStreamer *as, uint8_t au_index)
{
	SYNCDEBUG();
	pa->act.func = _play_proceed;
	pa->as = as;
	pa->au_index = au_index;
	sdc_read(&as->sdc, 0, &pa->act);
}

void _play_proceed(Activation *act)
{
	SYNCDEBUG();
	PlayAct *pa = (PlayAct *) act;
	AuIndexRec *ai = (AuIndexRec*) pa->as->sdc.blockbuffer;
#if 0
	int i;
	for (i=0; i<40; i++)
	{
		syncdebug(3, '\\', pa->as->sdc.blockbuffer[i]);
	}
#endif
	AuIndexRec *airec = &ai[pa->au_index];
#if 0
	syncdebug(2, 'x', pa->au_index);
	syncdebug(2, 't', 0xfeed);
	void *x2 = airec;
	void *x1 = ai;
	syncdebug(2, 'r', (uint16_t) (x2-x1));
	syncdebug32(2, 'o', airec->start_offset);
	syncdebug(2, 'b', airec->block_offset);
	syncdebug32(2, 'e', airec->end_offset);
#endif
	as_play(pa->as, airec->start_offset, airec->block_offset, airec->end_offset, NULL);
}

struct s_CmdProc;

typedef struct {
	Activation act;
	struct s_CmdProc *cp;
	int count;
} StreamTest;

typedef struct s_CmdProc {
	Activation act;
	SerialCmdAct sca;
	StreamTest stream_test;
	AudioStreamer audio_streamer;
	PlayAct play_act;
} CmdProc;


void cmdproc_update(Activation *act)
{
	CmdProc *cp = (CmdProc *) act;
	char *buf = cp->sca.cmd;

	SYNCDEBUG();

	if (strcmp(buf, "as\n")==0)
	{
		init_audio_streamer(&cp->audio_streamer, TIMER2);
	}
	else if (strcmp(buf, "aodebug\n")==0)
	{
		syncdebug(0, 'i', cp->audio_streamer.audio_out.index);
	}
	else if (strcmp(buf, "tidebug\n")==0)
	{
#ifndef SIM
		syncdebug(0, 't', TIFR2);
		syncdebug(0, 'm', TIMSK2);
		syncdebug(0, 'a', TCCR2A);
		syncdebug(0, 'b', TCCR2B);
		syncdebug(0, 'o', OCR2A);
		syncdebug(0, 'n', TCNT2);
		syncdebug(0, 'v', TOV2);
#endif // SIM
	}
	else if (strncmp(buf, "play ", 5)==0)
	{
		SYNCDEBUG();
		uint8_t val = (buf[5]-'a');
		syncdebug(0, 'v', val);
		play(&cp->play_act, &cp->audio_streamer, val);
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
	SYNCDEBUG();
	cp->act.func = cmdproc_update;
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

#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

