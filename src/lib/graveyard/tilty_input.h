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

#include "periph/rocket/rocket.h"

#ifndef SIM
#include "core/hardware.h"
#endif

#include "vect3d.h"

typedef enum {
  // internal states
  ti_neutral = 0,
  ti_undef = 0xa0,
  ti_left = 0xa1,
  ti_right = 0xa2,
  ti_up = 0xa3,
  ti_down = 0xa4,

  // messages sent out. (clumsy).
  ti_enter_pov = 0xa5,
  ti_exit_pov = 0xa6,

  // modifier flag
  ti_proposed = 0x100,
} TiltyInputState;

typedef enum {
  tia_led_click,
  tia_led_proposal,
  tia_led_black,
} TiltyLEDPattern;

typedef struct {
  Vect3D *accelValue;
  UIEventHandler *event_handler;

  TiltyInputState curState;
  Time proposed_time;

  TiltyLEDPattern led_pattern;
  Time led_anim_start;
} TiltyInputAct;

void _tilty_input_issue_event(TiltyInputAct *tia, TiltyInputState evt);
void tilty_input_update(TiltyInputAct *tia);
void tilty_input_init(TiltyInputAct *tia, Vect3D *accelValue,
                      UIEventHandler *event_handler);
