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
