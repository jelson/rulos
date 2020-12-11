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

#include "core/clock.h"
#include "core/cpumon.h"
#include "core/hal.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/bss_canary/bss_canary.h"
#include "periph/input_controller/focus.h"
#include "periph/input_controller/input_controller.h"
#include "periph/quadknob/quadknob.h"
#include "periph/rasters/rasters.h"
#include "periph/remote_keyboard/remote_keyboard.h"
#include "periph/rocket/calculator.h"
#include "periph/rocket/display_aer.h"
#include "periph/rocket/display_compass.h"
#include "periph/rocket/display_docking.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/display_thruster_graph.h"
#include "periph/rocket/idle_display.h"
#include "periph/rocket/labeled_display.h"
#include "periph/rocket/lunar_distance.h"
#include "periph/rocket/numeric_input.h"
#include "periph/rocket/pong.h"
#include "periph/rocket/remote_uie.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/sequencer.h"

/**************************************************************************/

int main() {
  hal_init();
  hal_init_rocketpanel();
  bss_canary_init();
  init_clock(10000, TIMER1);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  Network network;
  init_twi_network(&network, 100, ROCKET1_ADDR);

  // install_handler(ADC, adc_handler);

  board_buffer_module_init();

  /*
          FocusManager fa;
          focus_init(&fa);
  */

  RemoteKeyboardSend rks;
  init_remote_keyboard_send(&rks, &network, ROCKET_ADDR, REMOTE_KEYBOARD_PORT);

  /*
          RemoteBBufRecv rbr;
          init_remote_bbuf_recv(&rbr, &network);
  */

  DisplayAzimuthElevationRoll daer;
  daer_init(&daer, 0, ((Time)5) << 20);

  DThrusterGraph dtg;
  dtg_init_remote(&dtg, 1, &network);

  Calculator calc;
  calculator_init(&calc, 2, NULL,
                  (FetchCalcDecorationValuesIfc*)&daer.decoration_ifc);

  CascadedInputInjector cii;
  init_cascaded_input_injector(&cii, (UIEventHandler*)&calc.focus,
                               &rks.forwardLocalStrokes);
  RemoteKeyboardRecv rkr;
  init_remote_keyboard_recv(&rkr, &network, (InputInjectorIfc*)&cii,
                            REMOTE_SUBFOCUS_PORT0);

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

  ScreenBlankerListener sbl;
  init_screenblanker_listener(&sbl, &network);

  /*
          DAER daer;
          daer_init(&daer, 0, ((Time)5)<<20);

          Calculator calc;
          calculator_init(&calc, 2, &fa,
                  (FetchCalcDecorationValuesIfc*) &daer.decoration_ifc);
  */

#if MEASURE_CPU_FOR_RULOS_PAPER
  DScrollMsgAct dsm;
  dscrlmsg_init(&dsm, 1, "bong", 100);
  IdleDisplayAct idle;
  idle_display_init(&idle, &dsm, &cpumon);
#endif  // MEASURE_CPU_FOR_RULOS_PAPER

  cpumon_main_loop();

  return 0;
}
