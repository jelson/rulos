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

#include "chip/sim/core/sim.h"
#include "core/clock.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/audio/audio_driver.h"
#include "periph/audio/audio_server.h"
#include "periph/audio/audio_streamer.h"
#include "periph/rocket/rocket.h"
#include "periph/sdcard/sdcard.h"
#include "periph/uart/uart_net_media.h"

//////////////////////////////////////////////////////////////////////////////
void audioled_init();
void audioled_set(r_bool red, r_bool yellow);
//////////////////////////////////////////////////////////////////////////////

#define AUDIO_LED_RED GPIO_D2
#define AUDIO_LED_YELLOW GPIO_D3

#ifndef SIM
#include "hardware.h"
#endif  // SIM

void audioled_init() {
#ifndef SIM
  gpio_make_output(AUDIO_LED_RED);
  gpio_make_output(AUDIO_LED_YELLOW);
#endif  // SIM
}

void audioled_set(r_bool red, r_bool yellow) {
#ifndef SIM
  gpio_set_or_clr(AUDIO_LED_RED, !red);
  gpio_set_or_clr(AUDIO_LED_YELLOW, !yellow);
#endif  // SIM
}

//////////////////////////////////////////////////////////////////////////////

#if 0
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
#endif

typedef struct {
  UartMedia uart_media;
  struct {
    MediaRecvSlot mrs;
    char mrs_buffer[20];
  };  // must stay together
  MediaStateIfc *media;
  char send_buffer[20];
} UartListener;

void _uart_listener_recv_done(MediaRecvSlot *mrs, uint8_t len);

void uart_listener_init(UartListener *ul) {
  ul->mrs.func = _uart_listener_recv_done;
  ul->mrs.capacity = sizeof(ul->mrs_buffer);
  ul->mrs.occupied = FALSE;
  ul->mrs.user_data = ul;
  ul->media = uart_media_init(&ul->uart_media, &ul->mrs);
}

void _uart_listener_recv_done(MediaRecvSlot *mrs, uint8_t len) {
  UartListener *ul = (UartListener *)mrs->user_data;
  memcpy(ul->send_buffer, ul->mrs_buffer, len);
  ul->mrs.occupied = FALSE;
  (ul->media->send)(ul->media, 0x01, ul->send_buffer, len, NULL, NULL);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  Activation act;
  UartListener *ul;
} Tick;

void _tick_update(Activation *act);

void tick_init(Tick *tick, UartListener *ul) {
  tick->ul = ul;
  tick->act.func = _tick_update;
  schedule_us(1000000, &tick->act);
}

void tick_say(Tick *tick, char *msg) {
  (tick->ul->media->send)(tick->ul->media, 0x02, msg, strlen(msg), NULL, NULL);
}

void _tick_update(Activation *act) {
  Tick *tick = (Tick *)act;
  tick_say(tick, "tick");
  schedule_us(1000000, &tick->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  CpumonAct cpumon;
  //	CmdProc cmdproc;
  UartListener uart_listener;
  Tick tick;
} MainContext;
MainContext mc;

void g_tick_say(char *msg) { tick_say(&mc.tick, msg); }

int main() {
  audioled_init();
  util_init();
  hal_init(bc_audioboard);
  init_clock(1000, TIMER1);

  audioled_set(0, 0);

  uart_listener_init(&mc.uart_listener);

  tick_init(&mc.tick, &mc.uart_listener);

  g_tick_say("hiya!");

  // needs to be early, because it initializes uart, which at the
  // moment I'm using for SYNCDEBUG(), including in init_audio_server.
  // cmdproc_init(&mc.cmdproc, &mc.aserv, &mc.network);

  // init_audio_server(&mc.aserv, &mc.network, TIMER2);

  cpumon_init(&mc.cpumon);  // includes slow calibration phase

#if 0
	board_buffer_module_init();
#endif
  cpumon_main_loop();

  return 0;
}
