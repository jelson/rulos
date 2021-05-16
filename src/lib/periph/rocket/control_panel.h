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

#pragma once

#include "periph/hpam/hpam.h"
#include "periph/remote_keyboard/remote_keyboard.h"
#include "periph/rocket/calculator.h"
#include "periph/rocket/disco.h"
#include "periph/rocket/display_docking.h"
#include "periph/rocket/display_thrusters.h"
#include "periph/rocket/idle.h"
#include "periph/rocket/pong.h"
#include "periph/rocket/remote_uie.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screen4.h"
#include "periph/rocket/screenblanker.h"
#include "periph/rocket/sequencer.h"
#include "periph/rocket/snakegame.h"

#define CONTROL_PANEL_HEIGHT       SCREEN4SIZE
#define CONTROL_PANEL_NUM_CHILDREN 6
#define CP_NO_CHILD                (0xff)

struct s_control_child;

typedef struct s_control_child {
  UIEventHandler *uie_handler;
  char *name;
} ControlChild;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  Calculator calc;
} CCLocalCalc;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  RemoteKeyboardSend rks;
  RemoteUIE ruie;
} CCRemoteCalc;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  Launch launch;
} CCLaunch;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  DDockAct dock;
} CCDock;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  Pong pong;
} CCPong;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  Snake snake;
} CCSnake;

typedef struct {
  UIEventHandler *uie_handler;
  const char *name;
  Disco disco;
} CCDisco;

typedef struct s_direct_injector {
  InputInjectorFunc injector_func;
  struct s_control_panel *cp;
} direct_injector_t;

typedef struct s_control_panel {
  UIEventHandlerFunc handler_func;
  Screen4 s4;
  direct_injector_t direct_injector;

  ControlChild *children[CONTROL_PANEL_NUM_CHILDREN];
  uint8_t child_count;
  uint8_t selected_child;
  uint8_t active_child;

  Booster booster;

  // actual children listed here to allocate their storage statically.
  CCLocalCalc cclc;
  CCRemoteCalc ccrc;
  CCLaunch ccl;
  CCDock ccdock;
  CCPong ccpong;
  CCSnake ccsnake;
  CCDisco ccdisco;

  Keystroke vol_up_key;
  Keystroke vol_down_key;
  InputInjectorIfc *volume_input_ifc;

  IdleAct *idle;
} ControlPanel;

void init_control_panel(ControlPanel *cp, uint8_t board0, uint8_t aux_board0,
                        Network *network, HPAM *hpam, AudioClient *audioClient,
                        IdleAct *idle, ScreenBlanker *screenblanker,
                        JoystickState_t *joystick,
                        ThrusterState_t *thrusterState, Keystroke vol_up_key,
                        Keystroke vol_down_key,
                        InputInjectorIfc *volume_input_ifc /*optional*/,
                        FetchCalcDecorationValuesIfc *decoration_ifc);
