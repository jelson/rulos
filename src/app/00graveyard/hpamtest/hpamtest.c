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

#include "core/board_defs.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/hpam/hpam.h"
#include "periph/joystick/joystick.h"
#include "periph/rocket/rocket.h"

struct {
  char *name;
  HPAMIndex hpamIndex;
} hpams[] = {
    {"hobbs", HPAM_HOBBS},
    //	{"clanger", HPAM_RESERVED},
    //{"hatch", hpam_hatch_solenoid_reserved},
    {"lights", HPAM_LIGHTING_FLICKER},
    {"th_frLft", HPAM_THRUSTER_FRONTLEFT},
    {"th_frRgt", HPAM_THRUSTER_FRONTRIGHT},
    {"th_rear", HPAM_THRUSTER_REAR},
    {"booster", HPAM_BOOSTER},
    {NULL, 0},
};

char *idx_to_name(HPAMIndex idx) {
  int i;
  for (i = 0; i < HPAM_END; i++) {
    if (hpams[i].hpamIndex == idx) {
      return hpams[i].name;
    }
  }
  return "none";
}

typedef struct {
  JoystickState_t js;
  int8_t dir;
  bool btn;
  uint8_t idx;
  ThrusterUpdate *tu;
  HPAM hpam;
  BoardBuffer idx_bbuf;
  BoardBuffer state_bbuf;
} HTAct;

void ht_update(HTAct *ht) {
  schedule_us((Time)10000, (ActivationFuncPtr)ht_update, ht);

#define DBG_BONK 0
#if DBG_BONK
  memset(ht->state_bbuf.buffer, 0, NUM_DIGITS);
  ht->state_bbuf.buffer[NUM_DIGITS - 1] =
      ascii_to_bitmap('0' + ((clock_time_us() >> 20) & 7));
  ht->state_bbuf.buffer[NUM_DIGITS - 2] = ascii_to_bitmap('t');
  ht->state_bbuf.buffer[NUM_DIGITS - 4] =
      ascii_to_bitmap(hal_read_joystick_button() ? '*' : '_');
  board_buffer_draw(&ht->state_bbuf);
  return;
#endif  // DBG_BONK

  joystick_poll(&ht->js);
  int8_t new_dir = (ht->js.state & JOYSTICK_STATE_LEFT)
                       ? -1
                       : (ht->js.state & JOYSTICK_STATE_RIGHT) ? +1 : 0;
  if (new_dir != ht->dir) {
    if (ht->dir == -1) {
      ht->idx = (ht->idx - 1 + HPAM_END) % HPAM_END;
    } else if (ht->dir == +1) {
      ht->idx = (ht->idx + 1 + HPAM_END) % HPAM_END;
    }
    ht->dir = new_dir;
  }
  bool new_btn = (ht->js.state & JOYSTICK_STATE_TRIGGER) != 0;
  if (new_btn != ht->btn) {
    if (!ht->btn) {
      hpam_set_port(&ht->hpam, ht->idx, !hpam_get_port(&ht->hpam, ht->idx));
    }
    ht->btn = new_btn;
  }

  memset(ht->idx_bbuf.buffer, 0, NUM_DIGITS);
  ascii_to_bitmap_str(ht->idx_bbuf.buffer, NUM_DIGITS, idx_to_name(ht->idx));
  board_buffer_draw(&ht->idx_bbuf);

  memset(ht->state_bbuf.buffer, 0, NUM_DIGITS);
  ascii_to_bitmap_str(ht->state_bbuf.buffer, NUM_DIGITS,
                      hpam_get_port(&ht->hpam, ht->idx) ? "on" : "off");
  ht->state_bbuf.buffer[NUM_DIGITS - 1] =
      ascii_to_bitmap('0' + ((clock_time_us() >> 20) & 7));
  ht->state_bbuf.buffer[NUM_DIGITS - 2] = ascii_to_bitmap('t');
  ht->state_bbuf.buffer[NUM_DIGITS - 3] = ascii_to_bitmap(new_btn ? '-' : '_');
  ht->state_bbuf.buffer[NUM_DIGITS - 4] =
      ascii_to_bitmap(hal_read_joystick_button() ? '*' : '_');
  board_buffer_draw(&ht->state_bbuf);
}

void ht_init(HTAct *ht) {
  ht->js.x_adc_channel = 3;
  ht->js.y_adc_channel = 2;
  ht->dir = 0;
  ht->btn = FALSE;
  ht->idx = 0;
  ht->tu = NULL;
  init_hpam(&ht->hpam, 7, ht->tu);
  joystick_init(&ht->js);
  board_buffer_init(&ht->idx_bbuf);
  board_buffer_push(&ht->idx_bbuf, 3);
  board_buffer_init(&ht->state_bbuf);
  board_buffer_push(&ht->state_bbuf, 4);
  schedule_us((Time)10000, (ActivationFuncPtr)ht_update, ht);
}

int main() {
  rulos_hal_init();
  hal_init_rocketpanel();
  init_clock(10000, TIMER1);
  board_buffer_module_init();

  HTAct ht;
  ht_init(&ht);

  scheduler_run();

  return 0;
}
