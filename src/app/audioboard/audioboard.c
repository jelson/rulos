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

#include "core/clock.h"
#include "core/network.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/audio/audio_server.h"
#include "periph/uart/serial_console.h"

#if SIM
#include "chip/sim/core/sim.h"
#endif

typedef struct {
  AudioServer aserv;
  Network network;
  int foo;
} MainContext;
MainContext mc;

void init_audio_server_delayed_start(void *state) {
  MainContext *mc = (MainContext *)state;

  mc->foo--;
  if (mc->foo == 0) {
    init_audio_server(&mc->aserv, &mc->network, TIMER2);
  } else {
    schedule_us(1000000, init_audio_server_delayed_start, mc);
  }
}

int main() {
  hal_init();
  init_clock(1000, TIMER1);

#ifdef LOG_TO_SERIAL
  HalUart uart;
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_twi_network(&mc.network, 100, AUDIO_ADDR);

  mc.foo = 2;
  schedule_us(1000000, init_audio_server_delayed_start, &mc);

  cpumon_main_loop();

  return 0;
}
