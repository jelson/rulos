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

HalUart uart;

int main() {
  hal_init();
  hal_init_rocketpanel();

  hal_uart_init(&uart, 38400, true, /* uart_id= */ 0);
  LOG("Rocket dongle running");

  /*board_say(" rEAdy  "); */

  init_clock(10000, TIMER1);

  Network net;
  init_twi_network(&net, 100, DONGLE_BASE_ADDR + DONGLE_BOARD_ID);

  RemoteBBufRecv remoteBBufRecv;
  init_remote_bbuf_recv(&remoteBBufRecv, &net);

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();

  return 0;
}
