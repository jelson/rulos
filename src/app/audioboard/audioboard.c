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


struct s_CmdProc;

typedef struct s_CmdProc {
	Activation act;
	SerialCmdAct sca;
	Network *network;
	AudioServer *audio_server;
} CmdProc;

void cmdproc_update(Activation *act)
{
	CmdProc *cp = (CmdProc *) act;
	char *buf = cp->sca.cmd;

	SYNCDEBUG();

	if (strcmp(buf, "init\n")==0)
	{
		init_audio_server(cp->audio_server, cp->network, TIMER2);
	}
	else if (strcmp(buf, "fetch\n")==0)
	{
		_aserv_fetch_start(&cp->audio_server->fetch_start);
	}
	else if (strncmp(buf, "play ", 5)==0)
	{
		SYNCDEBUG();
		SoundToken skip = (buf[5]-'b');
		SoundToken loop = (buf[6]-'b');
		syncdebug(0, 's', skip);
		syncdebug(0, 'l', loop);
		_aserv_skip_to_clip(cp->audio_server, skip, loop);
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

void cmdproc_init(CmdProc *cp, AudioServer *audio_server, Network *network)
{
	cp->audio_server = audio_server;
	cp->network = network;
	serialcmd_init(&cp->sca, &cp->act);
	SYNCDEBUG();
	cp->act.func = cmdproc_update;
	SYNCDEBUG();
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	CpumonAct cpumon;
	AudioServer aserv;
	Network network;
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

	// needs to be early, because it initializes uart, which at the
	// moment I'm using for SYNCDEBUG(), including in init_audio_server.
	cmdproc_init(&mc.cmdproc, &mc.aserv, &mc.network);

	init_network(&mc.network, AUDIO_ADDR);

	init_audio_server(&mc.aserv, &mc.network, TIMER2);

	cpumon_init(&mc.cpumon);	// includes slow calibration phase


#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

