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

#include "core/rulos.h"
#include "periph/7seg_panel/7seg_panel.h"

/************************************************************************************/
/************************************************************************************/

typedef struct {
  int stage;
  BoardBuffer b[8];
  SSBitmap b_bitmap, d_bitmap;
} BoardIDAct_t;

static void update(BoardIDAct_t *ba) {
  uint8_t board, digit;

  for (board = 0; board < NUM_LOCAL_BOARDS; board++) {
    switch (ba->stage) {
      case 0:
        for (digit = 0; digit < NUM_DIGITS; digit++)
          ba->b[board].buffer[digit] = ba->b_bitmap;
        break;

      case 1:
        for (digit = 0; digit < NUM_DIGITS; digit++)
          ba->b[board].buffer[digit] = ascii_to_bitmap(board + '0');
        break;

      case 2:
        for (digit = 0; digit < NUM_DIGITS; digit++)
          ba->b[board].buffer[digit] = ba->d_bitmap;
        break;

      case 3:
        for (digit = 0; digit < NUM_DIGITS; digit++)
          ba->b[board].buffer[digit] = ascii_to_bitmap(digit + '0');
        break;

      default:
        for (digit = 0; digit < NUM_DIGITS; digit++)
          ba->b[board].buffer[digit] = (1 << (11 - ba->stage));
    }

    board_buffer_draw(&ba->b[board]);
  }
  if (++(ba->stage) == 12) ba->stage = 0;

  schedule_us(1000000, (ActivationFuncPtr)update, ba);
  return;
}

int main() {
  hal_init();
  hal_init_rocketpanel();
  init_clock(100000, TIMER1);
  board_buffer_module_init();

  BoardIDAct_t ba;
  ba.stage = 0;
  ba.b_bitmap = ascii_to_bitmap('b');
  ba.d_bitmap = ascii_to_bitmap('d');

  uint8_t board;
  for (board = 0; board < NUM_LOCAL_BOARDS; board++) {
    board_buffer_init(&ba.b[board] DBG_BBUF_LABEL("testboard"));
    board_buffer_push(&ba.b[board], board);
  }

  schedule_us(1, (ActivationFuncPtr)update, &ba);

  //	KeyTestActivation_t kta;
  //	display_keytest_init(&kta, 7);

  cpumon_main_loop();
  return 0;
}
