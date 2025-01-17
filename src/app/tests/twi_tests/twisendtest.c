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

// Enable this if the sender is a rocket panel that can display a status message
// describing what message number it's currently sending.
//#define STATUS_TO_LOCAL_PANEL

#include <stdio.h>
#include <stdlib.h>

#include "core/rulos.h"
#include "core/twi.h"

#ifdef STATUS_TO_LOCAL_PANEL
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/7seg_panel/display_controller.h"
#endif

#define INTER_MESSAGE_DELAY_US 125000

void test_without_netstack() {
  char inbuf[200];
  MediaRecvSlot *trs = (MediaRecvSlot *)inbuf;
  trs->capacity = sizeof(inbuf) - sizeof(MediaRecvSlot);

  MediaStateIfc *media = hal_twi_init(100, 0x8, trs);
  const unsigned char msg[] = "hello";
  (media->send)(media, 0x1, msg, 5, NULL, NULL);
  while (1) {
  }

  // sprintf(buf, "hi%3d", i);
  // ascii_to_bitmap_str(bm, 8, buf);
  // program_board(2, bm);
  //    hal_delay_ms(1000);
}

typedef struct {
  Network *net;
  SendSlot *sendSlot;
  int i;
} sendAct_t;

#ifdef STATUS_TO_LOCAL_PANEL
void board_say(const char *s) {
  SSBitmap bm[8];
  int i;

  ascii_to_bitmap_str(bm, 8, s);

  for (i = 0; i < 8; i++) {
    display_controller_program_board(i, bm);
  }
}
#endif

void sendMessage(sendAct_t *sa) {
  sa->i = (sa->i + 1) % 1000;

#ifdef STATUS_TO_LOCAL_PANEL
  char buf[9];
  strcpy(buf, "sENd    ");
  debug_itoha(&buf[4], sa->i);
  board_say(buf);
#endif

  if (sa->sendSlot->sending) {
    LOG("twisendtest: can't send message: outgoing buffer is busy");
  } else {
    char buf[8];
    strcpy(buf, "HELLO");
    int_to_string2(&buf[5], 3, 0, sa->i);
    sa->sendSlot->payload_len = strlen(buf);
    memcpy(&sa->sendSlot->wire_msg->data, buf, sa->sendSlot->payload_len);
    net_send_message(sa->net, sa->sendSlot);
    LOG("sent message");
  }

  schedule_us(INTER_MESSAGE_DELAY_US, (ActivationFuncPtr)sendMessage, sa);
}

char sendslot_storage[30];

void test_netstack() {
  Network net;

  init_clock(10000, TIMER1);

  SendSlot sendSlot;

  sendSlot.func = NULL;
  sendSlot.dest_addr = AUDIO_ADDR;
  sendSlot.wire_msg = (WireMessage *)sendslot_storage;
  sendSlot.wire_msg->dest_port = AUDIO_PORT;
  sendSlot.sending = FALSE;

  init_twi_network(&net, 100, 0x5);

  sendAct_t sa;
  sa.net = &net;
  sa.sendSlot = &sendSlot;
  sa.i = 0;

  schedule_us(INTER_MESSAGE_DELAY_US, (ActivationFuncPtr)sendMessage, &sa);

  scheduler_run();
  assert(FALSE);
}

int main() {
  rulos_hal_init();

#ifdef STATUS_TO_LOCAL_PANEL
  hal_init_rocketpanel();
  board_say("  InIt  ");
#endif
  // test_without_netstack();
  test_netstack();

  return 0;
}
