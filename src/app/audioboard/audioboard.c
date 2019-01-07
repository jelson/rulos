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

#include "core/clock.h"
#include "core/network.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/audio/audio_driver.h"
#include "periph/audio/audio_server.h"
#include "periph/audio/audio_streamer.h"
#include "periph/audio/audioled.h"
#include "periph/sdcard/sdcard.h"
#include "periph/uart/serial_console.h"

#if SIM
#include "chip/sim/core/sim.h"
#endif

SerialConsole *g_serial_console = NULL;


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
	serial_console_sync_send(g_serial_console, buf, strlen(buf));
}

void syncdebug32(uint8_t spaces, char f, uint32_t line)
{
	syncdebug(spaces, f, line>>16);
	syncdebug(spaces, '~', line&0x0ffff);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	SerialConsole sca;
	Network *network;
	AudioServer *audio_server;
	CpumonAct cpumon;
} CmdProc;

void cmdproc_update(CmdProc *cp)
{
	char *buf = cp->sca.line;
	audioled_set(1, 0);

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
		aserv_fetch_start(cp->audio_server);
	}
	else if (strncmp(buf, "play ", 5)==0)
	{
		SYNCDEBUG();
		SoundToken skip = (SoundToken) (buf[5]-'b');
		SoundToken loop = (SoundToken) (buf[6]-'b');
		syncdebug(0, 's', skip);
		syncdebug(0, 'l', loop);
		aserv_dbg_play(cp->audio_server, skip, loop);
	}
	else if (strncmp(buf, "vol ", 4)==0)
	{
		uint8_t v = atoi_hex(&buf[4]);
		as_set_volume(&cp->audio_server->audio_streamer, v);
		syncdebug(0, 'V', v);
	}
#if 0
	else if (strncmp(buf, "spiact", 4)==0)
	{
		syncdebug(3, 'a', (int) (cp->audio_server->audio_streamer.sdc.spi.spiact.act.func)<<1);
	}
#endif
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
#if 0
		SYNCDEBUG();
		char stack[0];
		syncdebug(3, 's', (uint16_t) (int) &stack);
		syncdebug(3, 'b', uart_busy(&cp->sca.uart));
		syncdebug(3, 'c', (int) cp);
		syncdebug(3, 'u', (int) &cp->sca.uart);
		syncdebug(3, 'o', (int) &cp->sca.uart.out_buf);
		syncdebug(3, 'c', (int) cp->sca.uart.out_buf[0]);
		syncdebug(3, 'c', (int) cp->sca.uart.out_buf[1]);
		syncdebug(3, 'c', (int) cp->sca.uart.out_buf[2]);
		syncdebug(3, 'c', (int) cp->sca.uart.out_buf[3]);

		bool x=true;
		while (uart_busy(&cp->sca.uart)) { x=!x; audioled_set(x, 0); }
		audioled_set(0, 0);
		int y=uart_busy(&cp->sca.uart);
		audioled_set(1, 1);
		syncdebug(3, 'b', y);
		audioled_set(1, 0);
#endif
		serial_console_sync_send(&cp->sca, reply_buf, strlen(reply_buf));
#if 0
		char *foo = (char*) "foo\n";
		serial_console_sync_send(&cp->sca, foo, strlen(foo));
		syncdebug(3, 'b', uart_busy(&cp->sca.uart));
#endif
	}
	audioled_set(1, 1);
	SYNCDEBUG();
}

void cmdproc_init(CmdProc *cp, AudioServer *audio_server, Network *network)
{
	cp->audio_server = audio_server;
	cp->network = network;

	serial_console_init(&cp->sca, (ActivationFuncPtr) cmdproc_update, cp);
	g_serial_console = &cp->sca;
	SYNCDEBUG();
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint8_t val;
} BlinkAct;

void _update_blink(BlinkAct *ba)
{
	ba->val = !ba->val;
	//audioled_set(ba->val, 0);
#ifndef SIM
//	gpio_set_or_clr(AUDIO_LED_RED, !ba->val);
#endif //!SIM
#if 0
	SYNCDEBUG();

#if !SIM
	extern uint8_t bss_end[0];
	syncdebug(10, 'b', bss_end[0]);
#endif // !SIM
#endif

	schedule_us(500000, (ActivationFuncPtr) _update_blink, ba);
}

void blink_init(BlinkAct *ba)
{
	schedule_us(100000, (ActivationFuncPtr) _update_blink, ba);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint8_t val;
} DACTest;

void _dt_update(DACTest *dt);

void dt_init(DACTest *dt)
{
	dt->val = 0;
	hal_audio_init();
	schedule_us(1, (ActivationFuncPtr) _dt_update, &dt);
}

void _dt_update(DACTest *dt)
{
	hal_audio_fire_latch();
	audioled_set((dt->val & 0x80)!=0, (dt->val & 0x40)!=0);
	if (dt->val==0) { dt->val = 128; }
	else if (dt->val==128) { dt->val = 255; }
	else { dt->val = 0; }
	schedule_us(1000000, (ActivationFuncPtr) _dt_update, &dt);
	hal_audio_shift_sample(dt->val);
}


//////////////////////////////////////////////////////////////////////////////

typedef struct {
	AudioServer aserv;
	Network network;
	CmdProc cmdproc;
	int foo;
} MainContext;
MainContext mc;

void init_audio_server_delayed_start(void *state)
{
	MainContext *mc = (MainContext *) state;

	SYNCDEBUG();
	mc->foo--;
	if (mc->foo==0)
	{
		SYNCDEBUG();
		init_audio_server(&mc->aserv, &mc->network, TIMER2);
	}
	else
	{
		SYNCDEBUG();
		syncdebug(2, 'f', mc->foo);
		schedule_us(1000000, init_audio_server_delayed_start, mc);
	}
}

int main()
{
	audioled_init();
	hal_init();
	init_clock(1000, TIMER1);

	audioled_set(0, 0);

	// needs to be early, because it initializes uart, which at the
	// moment I'm using for SYNCDEBUG(), including in init_audio_server.
	cmdproc_init(&mc.cmdproc, &mc.aserv, &mc.network);

	init_twi_network(&mc.network, 100, AUDIO_ADDR);
	SYNCDEBUG();

	mc.foo = 2;
	schedule_us(1000000, init_audio_server_delayed_start, &mc);
	SYNCDEBUG();

	BlinkAct ba;
	blink_init(&ba);

	cpumon_init(&mc.cmdproc.cpumon);	// includes slow calibration phase


#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

