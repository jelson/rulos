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

#include "chip/avr/periph/pov/pov.h"

#include <string.h>

void pov_update(PovHandler *pov);

void pov_init(PovHandler *pov, MirrorHandler *mirror, uint8_t laser_board,
              uint8_t laser_digit) {
  pov->func = (ActivationFunc)pov_update;
  pov->mirror = mirror;
  pov->laser_board = laser_board;
  pov->laser_digit = laser_digit;

  int i;
  for (i = 0; i < POV_BITMAP_LENGTH; i++) {
    pov->bitmap[i] = i & 0x0ff;
  }

  schedule_us(1, (Activation *)pov);
}

void pov_update(PovHandler *pov) {
  // update pov display at clock frequency
  schedule_us(1, (Activation *)pov);

  if (pov->mirror->period == 0) {
    // can't divide by that; wait.
    return;
  }

  Time now = clock_time_us();
  // TODO can we afford a divide on every update?
  uint32_t frac = ((now - pov->mirror->last_interrupt) * POV_BITMAP_LENGTH) /
                  pov->mirror->period;

  if (frac < 0 || frac > POV_BITMAP_LENGTH) {
    // out of bitmap range; ignore.
    return;
  }

  SSBitmap bm = pov->bitmap[frac];
  program_cell(pov->laser_board, pov->laser_digit, bm);
}
