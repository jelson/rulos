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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/board_defs.h"
#include "core/clock.h"
#include "core/cpumon.h"
#include "core/hal.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/7seg_panel/remote_bbuf.h"
#include "periph/bss_canary/bss_canary.h"
#include "periph/display_rtc/display_rtc.h"
#include "periph/input_controller/input_controller.h"
#include "periph/rasters/rasters.h"
#include "periph/remote_keyboard/remote_keyboard.h"
#include "periph/rocket/autotype.h"
#include "periph/rocket/control_panel.h"
#include "periph/rocket/display_aer.h"
#include "periph/rocket/display_compass.h"
#include "periph/rocket/display_docking.h"
#include "periph/rocket/display_gratuitous_graph.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/display_thruster_graph.h"
#include "periph/rocket/display_thrusters.h"
#include "periph/rocket/hobbs.h"
#include "periph/rocket/idle.h"
#include "periph/rocket/idle_display.h"
#include "periph/rocket/labeled_display.h"
#include "periph/rocket/lunar_distance.h"
#include "periph/rocket/numeric_input.h"
#include "periph/rocket/pong.h"
#include "periph/rocket/remote_uie.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screenblanker.h"
#include "periph/rocket/sequencer.h"
#include "periph/rocket/slow_boot.h"
#include "periph/rocket/volume_control.h"
#include "periph/uart/uart.h"

/****************************************************************************/

#if defined(JOYSTICK_X_CHAN) && defined(JOYSTICK_Y_CHAN)
#include "periph/joystick_adc/joystick_adc.h"
#define USE_JOYSTICK_ADC 1
#else
#include "periph/joystick_usb/joystick_usb.h"
#include "periph/max3421e/max3421e.h"
#define USE_JOYSTICK_USB 1
#endif

typedef struct {
  DRTCAct dr;
  Network network;
  LunarDistance ld;
  AudioClient audio_client;
  HPAM hpam;
  ControlPanel cp;
  InputPollerAct ip;
  RemoteKeyboardRecv rkr;
  ThrusterUpdate thrusterUpdate[4];
  ThrusterState_t ts;
  IdleAct idle;
  Hobbs hobbs;
  ScreenBlanker screenblanker;
  SlowBoot slow_boot;
  QuadKnob volknob;
  VolumeControl volume_control;
  QuadKnob pongknob;
  RemoteBBufSend rbs;
  DisplayAzimuthElevationRoll daer;
  DThrusterGraph dtg;
#if USE_JOYSTICK_ADC
  Joystick_ADC_t joystick;
#else
  max3421e_t max;
  Joystick_USB_t joystick;
#endif
} Rocket0;

/*
 * south-side quads:
 *  a,b (overlaps menu keys)
 *  e,f (pong)
 *
 * north-side quads:
 *  j,k (volume)
 *  m,n (pong)
 */

#define KEY_VOL_DOWN KeystrokeCtor('j')
#define KEY_VOL_UP KeystrokeCtor('k')
#define KEY_NPONG_LEFT KeystrokeCtor('m')
#define KEY_NPONG_RIGHT KeystrokeCtor('n')

#if defined(BOARD_LPEM2)
IOPinDef pin_vol_q0 = PINDEF(GPIO_A6);
IOPinDef pin_vol_q1 = PINDEF(GPIO_A7);
IOPinDef pin_npong_q0 = PINDEF(GPIO_A4);
IOPinDef pin_npong_q1 = PINDEF(GPIO_A5);
#elif defined(BOARD_STMPEM_REVA)
IOPinDef pin_vol_q0 = PINDEF(GPIO_A0);
IOPinDef pin_vol_q1 = PINDEF(GPIO_A1);
IOPinDef pin_npong_q0 = PINDEF(GPIO_A2);
IOPinDef pin_npong_q1 = PINDEF(GPIO_A3);
#elif defined(SIM)
// nothing
#else
#error "No pins defined for quads"
#include <stophere>
#endif

#define USE_LOCAL_KEYPAD 0

void init_rocket0(Rocket0 *r0) {
  init_twi_network(&r0->network, 200, ROCKET_ADDR);
  init_remote_bbuf_send(&r0->rbs, &r0->network);
  install_remote_bbuf_send(&r0->rbs);
  drtc_init(&r0->dr, 0, clock_time_us() + 20000000);
  lunar_distance_init(&r0->ld, 1, 2 /*, SPEED_POT_CHANNEL*/);
  init_audio_client(&r0->audio_client, &r0->network);
  memset(&r0->thrusterUpdate, 0, sizeof(r0->thrusterUpdate));
  init_hpam(&r0->hpam, 9, r0->thrusterUpdate);
  init_idle(&r0->idle);
#if USE_JOYSTICK_ADC
  init_joystick_adc(&r0->joystick, JOYSTICK_X_CHAN, JOYSTICK_Y_CHAN);
#else
  max3421e_init(&r0->max);
  init_joystick_usb(&r0->joystick, &r0->max);
#endif
  thrusters_init(&r0->ts, 9, (JoystickState_t *)&r0->joystick, &r0->hpam,
                 &r0->idle, &r0->audio_client);
  init_screenblanker(&r0->screenblanker, &r0->hpam, &r0->idle);

  volume_control_init(&r0->volume_control, &r0->audio_client,
                      /*board*/ 0, KEY_VOL_UP, KEY_VOL_DOWN);

  daer_init(&r0->daer, 10, ((Time)5) << 20);

  init_control_panel(&r0->cp, 3, 1, &r0->network, &r0->hpam, &r0->audio_client,
                     &r0->idle, &r0->screenblanker,
                     (JoystickState_t *)&r0->ts.joystick, &r0->ts, KEY_VOL_UP,
                     KEY_VOL_DOWN, &r0->volume_control.injector.iii,
                     (FetchCalcDecorationValuesIfc *)&r0->daer.decoration_ifc);
  r0->cp.ccl.launch.main_rtc = &r0->dr;
  r0->cp.ccl.launch.lunar_distance = &r0->ld;

  // Local input poller
#if USE_LOCAL_KEYPAD == 1 || defined(SIM)
  input_poller_init(&r0->ip, (InputInjectorIfc *)&r0->cp.direct_injector);
#endif

  dtg_init_local(&r0->dtg, 11);

  // Remote receiver
  init_remote_keyboard_recv(&r0->rkr, &r0->network,
                            (InputInjectorIfc *)&r0->cp.direct_injector,
                            REMOTE_KEYBOARD_PORT);

  // Register callbacks that should be fired when thruster state is updated
  r0->thrusterUpdate[0].func = (ThrusterUpdateFunc)dtg_update_state;
  r0->thrusterUpdate[0].data = &r0->dtg;

  r0->thrusterUpdate[1].func = (ThrusterUpdateFunc)ddock_thruster_update;
  r0->thrusterUpdate[1].data = &r0->cp.ccdock.dock;

  // r0->thrusterUpdate[2].func = idle_thruster_listener_func;
  // r0->thrusterUpdate[2].data = &r0->idle;

  init_hobbs(&r0->hobbs, &r0->hpam, &r0->idle);

  init_slow_boot(&r0->slow_boot, &r0->screenblanker, &r0->audio_client);

  init_quadknob(&r0->volknob, (InputInjectorIfc *)&r0->cp.direct_injector,
#ifndef SIM
                &pin_vol_q0, &pin_vol_q1,
#endif
                KEY_VOL_UP, KEY_VOL_DOWN);

  init_quadknob(&r0->pongknob, (InputInjectorIfc *)&r0->cp.direct_injector,
#ifndef SIM
                &pin_npong_q0, &pin_npong_q1,
#endif
                KEY_NPONG_LEFT, KEY_NPONG_RIGHT);

  bss_canary_init();
}

static Rocket0 rocket0;  // allocate obj in .bss so it's easy to count
HalUart uart;
CpumonAct cpumon;

int main() {
  hal_init();

  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");

#if NUM_LOCAL_BOARDS > 0
  hal_init_rocketpanel();
#endif

  // start the jiffy clock
  init_clock(10000, TIMER1);

  // includes slow calibration phase
  cpumon_init(&cpumon);

  board_buffer_module_init();
  init_rocket0(&rocket0);

#ifdef AUTOTYPE
  Autotype autotype;
  init_autotype(&autotype, (InputInjectorIfc *)&rocket0.cp.direct_injector,
                //"000aaaaac004671c",
                "000bc", (Time)1300000);
#endif

#if MEASURE_CPU_FOR_RULOS_PAPER
  DScrollMsgAct dsm;
  dscrlmsg_init(&dsm, 2, "bong", 100);
  IdleDisplayAct idle;
  idle_display_init(&idle, &dsm, &cpumon);
#endif  // MEASURE_CPU_FOR_RULOS_PAPER

  cpumon_main_loop();

  return 0;
}
