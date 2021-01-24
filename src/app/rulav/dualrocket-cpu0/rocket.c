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

#include "periph/rocket/rocket.h"

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
#include "core/bss_canary.h"
#include "periph/display_rtc/display_rtc.h"
#include "periph/input_controller/focus.h"
#include "periph/input_controller/input_controller.h"
#include "periph/joystick_adc/joystick_adc.h"
#include "periph/rasters/rasters.h"
#include "periph/remote_keyboard/remote_keyboard.h"
#include "periph/rocket/autotype.h"
#include "periph/rocket/calculator.h"
#include "periph/rocket/control_panel.h"
#include "periph/rocket/display_aer.h"
#include "periph/rocket/display_compass.h"
#include "periph/rocket/display_docking.h"
#include "periph/rocket/display_gratuitous_graph.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/display_thrusters.h"
#include "periph/rocket/hobbs.h"
#include "periph/rocket/idle.h"
#include "periph/rocket/idle_display.h"
#include "periph/rocket/labeled_display.h"
#include "periph/rocket/lunar_distance.h"
#include "periph/rocket/numeric_input.h"
#include "periph/rocket/pong.h"
#include "periph/rocket/potsticker.h"
#include "periph/rocket/remote_uie.h"
#include "periph/rocket/screenblanker.h"
#include "periph/rocket/sequencer.h"
#include "periph/rocket/slow_boot.h"
#include "periph/rocket/volume_control.h"
#include "periph/uart/uart.h"

#if SIM
#include "chip/sim/core/sim.h"
#endif

/****************************************************************************/
/****************************************************************************/

typedef struct {
  DRTCAct dr;
  Network network;
  LunarDistance ld;
  AudioClient audio_client;
  ThrusterUpdate thrusterUpdate[4];
  HPAM hpam;
  ControlPanel cp;
  InputPollerAct ip;
  RemoteKeyboardRecv rkr;
  Joystick_ADC_t joystick;
  ThrusterSendNetwork tsn;
  ThrusterState_t ts;
  IdleAct idle;
  Hobbs hobbs;
  ScreenBlanker screenblanker;
  ScreenBlankerSender screenblanker_sender;
  SlowBoot slow_boot;
  PotSticker volsticker;
  PotSticker pongsticker;
  VolumeControl volume_control;
  RemoteBBufSend rbs;
} Rocket0;

#define KEY_VOL_DOWN KeystrokeCtor('j')
#define KEY_VOL_UP KeystrokeCtor('k')
#define KEY_NPONG_LEFT KeystrokeCtor('m')
#define KEY_NPONG_RIGHT KeystrokeCtor('n')

#if !defined(JOYSTICK_X_CHAN) || !defined(JOYSTICK_Y_CHAN)
#error "JOYSTICK_X_CHAN and JOYSTICK_Y_CHAN must be defined."
#include <stophere>
#endif

#define POTSTICKER_CHANNEL 0
#define VOLUME_POT_CHANNEL 1
#define USE_LOCAL_KEYPAD 0

void init_rocket0(Rocket0 *r0) {
  init_twi_network(&r0->network, 200, ROCKET_ADDR);
  init_remote_bbuf_send(&r0->rbs, &r0->network);
  install_remote_bbuf_send(&r0->rbs);
  drtc_init(&r0->dr, 0, clock_time_us() + 20000000);
  lunar_distance_init(&r0->ld, 1, 2 /*, SPEED_POT_CHANNEL*/);
  init_audio_client(&r0->audio_client, &r0->network);
  memset(&r0->thrusterUpdate, 0, sizeof(r0->thrusterUpdate));
  init_hpam(&r0->hpam, 7, r0->thrusterUpdate);
  init_idle(&r0->idle);
  init_joystick_adc(&r0->joystick, JOYSTICK_X_CHAN, JOYSTICK_Y_CHAN);
  thrusters_init(&r0->ts, 7, (JoystickState_t *)&r0->joystick, &r0->hpam,
                 &r0->idle, &r0->audio_client);
  // r0->thrusterUpdate[2].func = idle_thruster_listener_func;
  // r0->thrusterUpdate[2].data = &r0->idle;

  init_screenblanker(&r0->screenblanker, &r0->hpam, &r0->idle);
  init_screenblanker_sender(&r0->screenblanker_sender, &r0->network);
  r0->screenblanker.screenblanker_sender = &r0->screenblanker_sender;

  volume_control_init(&r0->volume_control, &r0->audio_client,
                      /*board*/ 0, KEY_VOL_UP, KEY_VOL_DOWN);

  init_control_panel(&r0->cp, 3, 1, &r0->network, &r0->hpam, &r0->audio_client,
                     &r0->idle, &r0->screenblanker,
                     (JoystickState_t *) &r0->joystick,
                     &r0->ts, KEY_VOL_UP, KEY_VOL_DOWN,
                     &r0->volume_control.injector.iii,
                     NULL /* local calc decoration ifc */);
  r0->cp.ccl.launch.main_rtc = &r0->dr;
  r0->cp.ccl.launch.lunar_distance = &r0->ld;

  // Local input poller
#if USE_LOCAL_KEYPAD == 1 || defined(SIM)
  input_poller_init(&r0->ip, (InputInjectorIfc *)&r0->cp.direct_injector);
#endif

  // Remote receiver
  init_remote_keyboard_recv(&r0->rkr, &r0->network,
                            (InputInjectorIfc *)&r0->cp.direct_injector,
                            REMOTE_KEYBOARD_PORT);

  r0->thrusterUpdate[1].func = (ThrusterUpdateFunc)ddock_thruster_update;
  r0->thrusterUpdate[1].data = &r0->cp.ccdock.dock;

  init_thruster_send_network(&r0->tsn, &r0->network);
  r0->thrusterUpdate[0].func = (ThrusterUpdateFunc)tsn_update;
  r0->thrusterUpdate[0].data = &r0->tsn;

  init_hobbs(&r0->hobbs, &r0->hpam, &r0->idle);

  init_slow_boot(&r0->slow_boot, &r0->screenblanker, &r0->audio_client);

  init_potsticker(&r0->pongsticker, POTSTICKER_CHANNEL,
                  (InputInjectorIfc *)&r0->cp.direct_injector, 9,
                  KEY_NPONG_LEFT, KEY_NPONG_RIGHT);
  init_potsticker(&r0->volsticker, POTSTICKER_CHANNEL,
                  (InputInjectorIfc *)&r0->cp.direct_injector, 9, KEY_VOL_DOWN,
                  KEY_VOL_UP);
}

static Rocket0 rocket0;  // allocate obj in .bss so it's easy to count
HalUart uart;
CpumonAct cpumon;

int main() {
  hal_init();

  hal_uart_init(&uart, 38400, true, /* uart_id= */ 0);
  LOG("Log output running, %d local, %d remote", NUM_LOCAL_BOARDS,
      NUM_REMOTE_BOARDS);

  // Only init the rocketpanel module in the ROCKET0 configuration, not
  // the TWI-output-only NETROCKET configuration.
#ifdef BOARDCONFIG_ROCKET0
  hal_init_rocketpanel();
#endif

  // start the jiffy clock
  init_clock(10000, TIMER1);
  bss_canary_init();

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
