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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/clock.h"
#include "core/cpumon.h"
#include "core/hal.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/bss_canary/bss_canary.h"
#include "periph/input_controller/input_controller.h"
#include "periph/quadknob/quadknob.h"
#include "periph/remote_keyboard/remote_keyboard.h"

/************************************************************************/

HalUart uart;

int main() {
  hal_init();
  bss_canary_init();

  hal_uart_init(&uart, 38400, true, /* uart_id= */ 0);
  LOG("South bridge serial logging up");

  init_clock(10000, TIMER1);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  Network network;
  init_twi_network(&network, 100, ROCKET1_ADDR);

  RemoteKeyboardSend rks;
  init_remote_keyboard_send(&rks, &network, ROCKET_ADDR, REMOTE_KEYBOARD_PORT);

  InputPollerAct ip;
#define LOCALTEST 0
#if LOCALTEST
  input_poller_init(&ip, (InputInjectorIfc*)&cii);
#else
  input_poller_init(&ip, &rks.forwardLocalStrokes);
#endif

#ifndef SIM
  // IO pins aren't defined for SIM. Clunky. Could simulate them?
  IOPinDef q0pin0 = PINDEF(GPIO_C0);
  IOPinDef q0pin1 = PINDEF(GPIO_C1);
  QuadKnob q0;
  init_quadknob(&q0, &rks.forwardLocalStrokes, &q0pin0, &q0pin1,
    KeystrokeCtor('e'), KeystrokeCtor('f'));

  IOPinDef q1pin0 = PINDEF(GPIO_C2);
  IOPinDef q1pin1 = PINDEF(GPIO_C3);
  QuadKnob q1;
  init_quadknob(&q1, &rks.forwardLocalStrokes, &q1pin0, &q1pin1,
    KeystrokeCtor('a'), KeystrokeCtor('b'));
#endif

  cpumon_main_loop();

  return 0;
}
