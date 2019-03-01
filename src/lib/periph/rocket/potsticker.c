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

#include "periph/rocket/potsticker.h"

void ps_update(PotSticker *ps) {
  int16_t new_val = hal_read_adc(ps->adc_channel);
  int8_t new_left = (new_val + ps->hysteresis) / ps->detents;
  int8_t new_right = (new_val - ps->hysteresis) / ps->detents;

#if DEBUG_BRUTE_DISPLAY
  static char dbg_v = '_';
#endif

  if (new_left < ps->last_digital_value) {
    ps->ifi->func(ps->ifi, ps->back);
    ps->last_digital_value = new_left;
#if DEBUG_BRUTE_DISPLAY
    dbg_v = 'L';
#endif
  } else if (new_right > ps->last_digital_value) {
    ps->ifi->func(ps->ifi, ps->fwd);
    ps->last_digital_value = new_right;
#if DEBUG_BRUTE_DISPLAY
    dbg_v = 'R';
#endif
  }

#if DEBUG_BRUTE_DISPLAY
  char msg[8];
  int_to_string2(msg, 4, 0, ps->last_digital_value);
  msg[4] = dbg_v;
  int_to_string2(&msg[5], 3, 0, new_left);
  SSBitmap bm[8];
  ascii_to_bitmap_str(bm, 8, msg);
  program_board(2, bm);
#endif  // DEBUG_BRUTE_DISPLAY

  schedule_us(50000, (ActivationFuncPtr)ps_update, ps);
}

void init_potsticker(PotSticker *ps, uint8_t adc_channel, InputInjectorIfc *ifi,
                     uint8_t detents, Keystroke fwd, Keystroke back) {
  ps->adc_channel = adc_channel;
  hal_init_adc_channel(ps->adc_channel);
  ps->ifi = ifi;
  ps->detents = detents;
  ps->hysteresis = 1024 / detents / 4;
  ps->fwd = fwd;
  ps->back = back;

  ps->last_digital_value = hal_read_adc(ps->adc_channel) / ps->detents;

  schedule_us(1, (ActivationFuncPtr)ps_update, ps);
}
