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

#if SIM
#include "chip/sim/core/sim.h"
#endif

/************************************************************************************/
/************************************************************************************/

int main() {
  hal_init();
  bss_canary_init();
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
  init_quadknob(&q0, &rks.forwardLocalStrokes, &q0pin0, &q0pin1, 'r', 's');

  IOPinDef q1pin0 = PINDEF(GPIO_C2);
  IOPinDef q1pin1 = PINDEF(GPIO_C3);
  QuadKnob q1;
  init_quadknob(&q1, &rks.forwardLocalStrokes, &q1pin0, &q1pin1, 'a', 'b');
#endif

  cpumon_main_loop();

  return 0;
}
