/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
#include "periph/audio/audioled.h"
#include "periph/rocket/rocket.h"
#include "periph/sdcard/sdcard.h"
#include "periph/uart/uart_net_media.h"

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

void network_listener_init(NetworkListener *nl) {
  nl->recvSlot.func = _nl_recv_complete;
  nl->recvSlot.port = UARTNETWORKTEST_PORT;
  nl->recvSlot.msg = &nl->recvMessage;
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

void _nl_recv_complete(RecvSlot *recv_slot, uint8_t payload_size) {
  NetworkListener *nl = (NetworkListener *)recv_slot->user_data;
  nl_send_str(nl, nl->recvData, payload_size);
  nl->recvSlot.msg_occupied = FALSE;
}

// TODO we really should make this callback optional,
// and just set msg_occupied to FALSE in the network stack! I think that's
// all it's ever used for in practice; no one actually queues msgs within a
// task.
void _nl_send_complete(struct s_send_slot *send_slot) {
  send_slot->sending = FALSE;
}

void nl_send_str(NetworkListener *nl, char *str, uint8_t len) {
  if (nl->sendSlot.sending) {
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

void tick_init(Tick *tick, NetworkListener *nl) {
  tick->nl = nl;
  tick->act.func = _tick_update;
  schedule_us(1000000, &tick->act);
}

void tick_say(Tick *tick, char *msg) { nl_send_str(tick->nl, "tick", 4); }

void _tick_update(Activation *act) {
  Tick *tick = (Tick *)act;
  tick_say(tick, "tick");
  schedule_us(1000000, &tick->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  NetworkListener network_listener;
  Tick tick;
} MainContext;
MainContext mc;

void g_tick_say(char *msg) { tick_say(&mc.tick, msg); }

int main() {
  audioled_init();
  rulos_hal_init();
  init_clock(1000, TIMER1);

  audioled_set(0, 0);

  network_listener_init(&mc.network_listener);

  tick_init(&mc.tick, &mc.network_listener);

  g_tick_say("hiya!");

  scheduler_run();

  return 0;
}
