/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include <stdio.h>
#include <stdlib.h>

#include "core/rulos.h"
//#include "core/twi.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/7seg_panel/board_buffer.h"

#define INTER_MESSAGE_DELAY_US 125000

typedef struct {
  BoardBuffer *bb0, *bb4;
  int i;
} sendAct_t;

void sendMessage(sendAct_t *sa) {
  sa->i = (sa->i + 1) % 1000;

  char buf[9];
  strcpy(buf, "HiYa    ");
  debug_itoha(&buf[4], sa->i);

  ascii_to_bitmap_str(sa->bb0->buffer, 8, buf);
  board_buffer_draw(sa->bb0);

  ascii_to_bitmap_str(sa->bb4->buffer, 8, buf);
  board_buffer_draw(sa->bb4);

  schedule_us(INTER_MESSAGE_DELAY_US, (ActivationFuncPtr)sendMessage, sa);
}

int main() {
  hal_init();
  hal_init_rocketpanel();
  init_clock(10000, TIMER1);

  Network network;
  init_twi_network(&network, 100, 0x5);

  BoardBuffer bb0, bb4;
  sendAct_t sa = {&bb0, &bb4, 0};

  RemoteBBufSend remote_bbuf_send;

  board_buffer_module_init();
  init_remote_bbuf_send(&remote_bbuf_send, &network);
  install_remote_bbuf_send(&remote_bbuf_send);
  board_buffer_init(&bb0 DBG_BBUF_LABEL("main"));
  board_buffer_push(&bb0, 0);
  board_buffer_init(&bb4 DBG_BBUF_LABEL("remote"));
  board_buffer_push(&bb4, 4);

  sendMessage(&sa);

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();
  assert(FALSE);
}
