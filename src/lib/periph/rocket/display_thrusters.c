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

#include "periph/rocket/display_thrusters.h"
#include "periph/joystick/joystick.h"
#include "periph/rocket/rocket.h"

//#define LOG_JOYSTICK

static void thrusters_update(ThrusterState_t *ts) {
  // get state from joystick
  joystick_poll(ts->joystick);

#ifdef LOG_JOYSTICK
  static int i = 0;
  if (i++ == 5) {
    LOG("thrusters_update: x=%d, y=%d, trigger=%s", ts->joystick->x_pos,
        ts->joystick->y_pos,
        ts->joystick->state & JOYSTICK_STATE_TRIGGER ? "down" : "up");
    i = 0;
  }
#endif

  if (ts->joystick->state & JOYSTICK_STATE_DISCONNECTED) {
    ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, " NO");
    ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, "JOy");
  } else {
    if (ts->joystick->state & JOYSTICK_STATE_TRIGGER) {
      ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, "PSH");
      ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, "PSH");
    } else {
      char buf[4];

      int_to_string2(buf, 3, 2, ts->joystick->x_pos);
      ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, buf);
      int_to_string2(buf, 3, 2, ts->joystick->y_pos);
      ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, buf);
    }

    // display X and Y thresholds
    if (ts->joystick->state & JOYSTICK_STATE_LEFT)
      ts->bbuf.buffer[1] |= SSB_DECIMAL;
    else if (ts->joystick->state & JOYSTICK_STATE_RIGHT)
      ts->bbuf.buffer[3] |= SSB_DECIMAL;
    else
      ts->bbuf.buffer[2] |= SSB_DECIMAL;

    if (ts->joystick->state & JOYSTICK_STATE_DOWN)
      ts->bbuf.buffer[5] |= SSB_DECIMAL;
    else if (ts->joystick->state & JOYSTICK_STATE_UP)
      ts->bbuf.buffer[7] |= SSB_DECIMAL;
    else
      ts->bbuf.buffer[6] |= SSB_DECIMAL;
  }

  // fire thrusters, baby!!
  //
  // current logic: only one thruster is firing at a time.
  // UP         = thruster A.
  // DOWN&LEFT  = thruster B.
  // DOWN&RIGHT = thruster C.
  if (!ts->joystick_muted) {
    hpam_set_port(ts->hpam, HPAM_THRUSTER_REAR,
                  ts->joystick->state & JOYSTICK_STATE_UP);
    hpam_set_port(ts->hpam, HPAM_THRUSTER_FRONTRIGHT,
                  ts->joystick->state & JOYSTICK_STATE_DOWN &&
                      ts->joystick->state & JOYSTICK_STATE_LEFT);
    hpam_set_port(ts->hpam, HPAM_THRUSTER_FRONTLEFT,
                  ts->joystick->state & JOYSTICK_STATE_DOWN &&
                      ts->joystick->state & JOYSTICK_STATE_RIGHT);
  }

  if ((ts->joystick->state & (JOYSTICK_STATE_UP | JOYSTICK_STATE_LEFT |
                              JOYSTICK_STATE_RIGHT | JOYSTICK_STATE_TRIGGER)) !=
      0) {
    // LOG("idle touch due to nonzero joystick: %d",
    // ts->joystick->state);
    if (ts->idle != NULL) {
      idle_touch(ts->idle);
    }
  }

  // draw digits
  board_buffer_draw(&ts->bbuf);

  schedule_us(10000, (ActivationFuncPtr)thrusters_update, ts);
}

void thrusters_init(ThrusterState_t *ts, uint8_t board,
                    JoystickState_t *joystick, HPAM *hpam, IdleAct *idle) {
  board_buffer_init(&ts->bbuf DBG_BBUF_LABEL("thrusters"));
  // mask off HPAM digits, so HPAM 'display' shows through
  board_buffer_set_alpha(&ts->bbuf, 0x77);
  board_buffer_push(&ts->bbuf, board);

  ts->joystick = joystick;
  ts->joystick_muted = FALSE;
  ts->hpam = hpam;
  ts->idle = idle;

  schedule_us(1, (ActivationFuncPtr)thrusters_update, ts);
}

void mute_joystick(ThrusterState_t *ts) { ts->joystick_muted = TRUE; };

void unmute_joystick(ThrusterState_t *ts) { ts->joystick_muted = FALSE; };
