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

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/input_controller/focus.h"
#include "periph/joystick/joystick.h"
#include "periph/rocket/booster.h"
#include "periph/rocket/drift_anim.h"
#include "periph/rocket/screen4.h"
#include "periph/rocket/thruster_protocol.h"

#define DOCK_HEIGHT SCREEN4SIZE
#define MAX_Y (DOCK_HEIGHT * 6)
#define MAX_X (NUM_DIGITS * 4)
#define CTR_X (MAX_X / 2)
#define CTR_Y (MAX_Y / 2)

struct s_ddockact;

typedef struct {
  UIEventHandlerFunc func;
  struct s_ddockact *act;
} DDockHandler;

typedef struct s_ddockact {
  Screen4 *s4;
  DDockHandler handler;
  uint8_t focused;
  DriftAnim xd, yd, rd;
  uint32_t last_impulse_time;
  ThrusterPayload thrusterPayload;
  BoardBuffer auxboards[2];
  uint8_t auxboard_base;
  r_bool docking_complete;
  AudioClient *audioClient;
  Booster *booster;
  JoystickState_t *joystick;
} DDockAct;

void ddock_init(DDockAct *act, Screen4 *s4, uint8_t auxboard_base,
                AudioClient *audioClient, Booster *booster,
                JoystickState_t *joystick);
void ddock_reset(DDockAct *dd);
void ddock_thruster_update(DDockAct *act, ThrusterPayload *tp);
