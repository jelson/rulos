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

#include "core/rulos.h"
#include "core/twi.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/7seg_panel/remote_bbuf.h"

#define DONGLE_BOARD_ID 8

static void recv_func(RecvSlot *recvSlot, uint8_t payload_size) {
  rbr_recv(recvSlot, payload_size);
}

int main() {
  hal_init();
  hal_init_rocketpanel();

  char data[sizeof(Message) + sizeof(BBufMessage)];

  /*board_say(" rEAdy  "); */

  init_clock(10000, TIMER1);

  RecvSlot recvSlot;
  recvSlot.func = recv_func;
  recvSlot.port = REMOTE_BBUF_PORT;
  recvSlot.msg = (Message *)data;
  recvSlot.payload_capacity = sizeof(data) - sizeof(Message);
  recvSlot.msg_occupied = FALSE;

  Network net;
  init_twi_network(&net, 100, DONGLE_BASE_ADDR + DONGLE_BOARD_ID);
  net_bind_receiver(&net, &recvSlot);

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();

  return 0;
}
