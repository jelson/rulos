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
#include "uart_net_media.h"
#include "audioled.h"

typedef struct {
	UartMedia uart_media;
	Network network;

	struct {
		Message recvMessage;
		char recvData[30];
		RecvSlot recvSlot;
	};
	struct {
		Message sendMessage;
		char sendData[30];
		SendSlot sendSlot;
	};
} NetworkListener;

void _nl_recv_complete(RecvSlot *recv_slot, uint8_t payload_size);
void _nl_send_complete(struct s_send_slot *send_slot);
void nl_send_str(NetworkListener *nl, char *str, uint8_t len);

void network_listener_init(NetworkListener *nl)
{
	nl->recvSlot.func = _nl_recv_complete;
	nl->recvSlot.port = UARTNETWORKTEST_PORT;
	nl->recvSlot.msg =  &nl->recvMessage;
	nl->recvSlot.payload_capacity = sizeof(nl->recvData);
	nl->recvSlot.msg_occupied = FALSE;
	nl->recvSlot.user_data = nl;

	nl->sendSlot.func = _nl_send_complete;
	nl->sendSlot.dest_addr = 0x1;
	nl->sendSlot.msg = &nl->sendMessage;
	nl->sendSlot.sending = FALSE;

	MediaStateIfc *media = uart_media_init(&nl->uart_media, &nl->network.mrs);
	init_network(&nl->network, media);
	net_bind_receiver(&nl->network, &nl->recvSlot);
}

void _nl_recv_complete(RecvSlot *recv_slot, uint8_t payload_size)
{
	NetworkListener *nl = (NetworkListener *) recv_slot->user_data;
	nl_send_str(nl, nl->recvData, payload_size);
	nl->recvSlot.msg_occupied = FALSE;
}

// TODO we really should make this callback optional,
// and just set msg_occupied to FALSE in the network stack! I think that's
// all it's ever used for in practice; no one actually queues msgs within a
// task.
void _nl_send_complete(struct s_send_slot *send_slot)
{
	send_slot->sending = FALSE;
}

void nl_send_str(NetworkListener *nl, char *str, uint8_t len)
{
	if (nl->sendSlot.sending)
	{
		return;
	}
	memcpy(nl->sendData, str, len);
	nl->sendMessage.dest_port = 0x6;
	nl->sendMessage.checksum = 0;
	nl->sendMessage.payload_len = len;

	net_send_message(&nl->network, &nl->sendSlot);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	NetworkListener *nl;
} Tick;

void _tick_update(Activation *act);

void tick_init(Tick *tick, NetworkListener *nl)
{
	tick->nl = nl;
	tick->act.func = _tick_update;
	schedule_us(1000000, &tick->act);
}

void tick_say(Tick *tick, char *msg)
{
	nl_send_str(tick->nl, "tick", 4);
}

void _tick_update(Activation *act)
{
	Tick *tick = (Tick *) act;
	tick_say(tick, "tick");
	schedule_us(1000000, &tick->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	CpumonAct cpumon;
	NetworkListener network_listener;
	Tick tick;
} MainContext;
MainContext mc;

void g_tick_say(char *msg)
{
	tick_say(&mc.tick, msg);
}

int main()
{
	audioled_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

	audioled_set(0, 0);

	network_listener_init(&mc.network_listener);

	tick_init(&mc.tick, &mc.network_listener);

	g_tick_say("hiya!");

	cpumon_init(&mc.cpumon);	// includes slow calibration phase
	cpumon_main_loop();

	return 0;
}

