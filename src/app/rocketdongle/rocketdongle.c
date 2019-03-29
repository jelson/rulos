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
