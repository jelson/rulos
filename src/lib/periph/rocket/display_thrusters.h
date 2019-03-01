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

#include "core/network.h"
#include "periph/hpam/hpam.h"
#include "periph/joystick/joystick.h"
#include "periph/rocket/idle.h"

typedef struct {
  JoystickState_t joystick_state;
  BoardBuffer bbuf;
  ThrusterPayload payload;
  HPAM *hpam;
  IdleAct *idle;

  // True if the joystick is temporarily not affecting thrusters, i.e. if it's
  // under the control of something else.
  r_bool joystick_muted;
} ThrusterState_t;

void thrusters_init(ThrusterState_t *ts, uint8_t board, uint8_t x_chan,
                    uint8_t y_chan, HPAM *hpam, IdleAct *idle);

void mute_joystick(ThrusterState_t *ts);
void unmute_joystick(ThrusterState_t *ts);
