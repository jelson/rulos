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
	UartState_t uart;
	char cmd[80];
	char *cmd_ptr;
	Activation *cmd_act;
} SerialCmdAct;

void serialcmd_update(Activation *act)
{
	SerialCmdAct *sca = (SerialCmdAct *) act;

	char rcv_chr;
	if (CharQueue_pop(sca->uart.recvQueue.q, &rcv_chr)) {
		*(sca->cmd_ptr) = rcv_chr;
		audioled_set(1, (((int)sca->cmd_ptr) & 1));

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

SerialCmdAct *g_serialcmd = NULL;

void serialcmd_init(SerialCmdAct *sca, Activation *cmd_act)
{
	uart_init(&sca->uart, 38400, TRUE);
	sca->act.func = serialcmd_update;
	sca->cmd_ptr = sca->cmd;
	sca->cmd_act = cmd_act;

	g_serialcmd = sca;

	schedule_us(1000, &sca->act);
}

void serialcmd_sync_send(SerialCmdAct *act, char *buf, uint16_t buflen)
{
	uart_send(&act->uart, buf, buflen, NULL, NULL);
	while (uart_busy(&act->uart)) { }
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
	serialcmd_sync_send(g_serialcmd, buf, strlen(buf));
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
//	gpio_set_or_clr(AUDIO_LED_RED, !red);
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
	uint16_t volume;
	CpumonAct cpumon;
} CmdProc;

void cmdproc_update(Activation *act)
{
	CmdProc *cp = (CmdProc *) act;
	char *buf = cp->sca.cmd;

	SYNCDEBUG();
#if !SIM
	syncdebug(4, 'd', (uint16_t) (&cp->audio_server->audio_streamer.sdc));
#endif

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
		SoundCmd skip = {(buf[5]-'b')};
		SoundCmd loop = {(buf[6]-'b')};
		syncdebug(0, 's', skip.token);
		syncdebug(0, 'l', loop.token);
		_aserv_skip_to_clip(cp->audio_server, skip, loop);
	}
	else if (strncmp(buf, "vol ", 4)==0)
	{
		cp->volume = atoi_hex(&buf[4]);
		syncdebug(0, 'V', cp->volume);
	}
	else if (strncmp(buf, "spiact", 4)==0)
	{
		syncdebug(3, 'a', (int) (cp->audio_server->audio_streamer.sdc.spi.spiact.act.func)<<1);
	}
	else if (strncmp(buf, "idle", 4)==0)
	{
		syncdebug(0, 'I', cpumon_get_idle_percentage(&cp->cpumon));
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
	cp->volume = 256;
	SYNCDEBUG();
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} BlinkAct;

void _update_blink(Activation *act)
{
	BlinkAct *ba = (BlinkAct *) act;
	ba->val = !ba->val;
//	audioled_set(ba->val, 0);
#ifndef SIM
	gpio_set_or_clr(AUDIO_LED_RED, !ba->val);
#endif //!SIM
	schedule_us(1000000, &ba->act);
}

void blink_init(BlinkAct *ba)
{
	ba->act.func = _update_blink;
	schedule_us(1000000, &ba->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} DACTest;

void _dt_update(Activation *act);

void dt_init(DACTest *dt)
{
	dt->act.func = _dt_update;
	dt->val = 0;
	hal_audio_init();
	schedule_us(1, &dt->act);
}

void _dt_update(Activation *act)
{
	DACTest *dt = (DACTest *) act;
	hal_audio_fire_latch();
	audioled_set((dt->val & 0x80)!=0, (dt->val & 0x40)!=0);
	if (dt->val==0) { dt->val = 128; }
	else if (dt->val==128) { dt->val = 255; }
	else { dt->val = 0; }
	schedule_us(1000000, &dt->act);
	hal_audio_shift_sample(dt->val);
}


//////////////////////////////////////////////////////////////////////////////

typedef struct {
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

#if 1
	// needs to be early, because it initializes uart, which at the
	// moment I'm using for SYNCDEBUG(), including in init_audio_server.
	cmdproc_init(&mc.cmdproc, &mc.aserv, &mc.network);

	init_twi_network(&mc.network, AUDIO_ADDR);

	init_audio_server(&mc.aserv, &mc.network, TIMER2);
#else
	DACTest dt;
	dt_init(&dt);
#endif

	BlinkAct ba;
	blink_init(&ba);

	cpumon_init(&mc.cmdproc.cpumon);	// includes slow calibration phase


#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

