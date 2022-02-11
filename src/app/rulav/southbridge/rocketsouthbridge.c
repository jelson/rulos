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

#include "core/bss_canary.h"
#include "core/clock.h"
#include "core/hal.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/input_controller/input_controller.h"
#include "periph/quadknob/quadknob.h"
#include "periph/remote_keyboard/remote_keyboard.h"

/************************************************************************/

int main() {
  rulos_hal_init();
  bss_canary_init();

  UartState_t uart;
  uart_init(&uart, /* uart_id= */ 0, 115200);
  log_bind_uart(&uart);
  LOG("South bridge serial logging up");

  init_clock(10000, TIMER1);


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
  gpio_pin_t q0pin0 = GPIO_C0;
  gpio_pin_t q0pin1 = GPIO_C1;
  QuadKnob q0;
  init_quadknob(&q0, &rks.forwardLocalStrokes, q0pin0, q0pin1,
                KeystrokeCtor('e'), KeystrokeCtor('f'));

  gpio_pin_t q1pin0 = GPIO_C2;
  gpio_pin_t q1pin1 = GPIO_C3;
  QuadKnob q1;
  init_quadknob(&q1, &rks.forwardLocalStrokes, q1pin0, q1pin1,
                KeystrokeCtor('a'), KeystrokeCtor('b'));
#endif

  scheduler_run();

  return 0;
}
