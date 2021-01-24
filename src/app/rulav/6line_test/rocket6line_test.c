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

#include "periph/rocket6line/rocket6line.h"

#include "core/rulos.h"
#include "periph/7seg_panel/display_controller.h"
#include "core/bss_canary.h"

#define UPDATE_RATE_US 1000000

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif

static void test_mode_update(void *data) {
  schedule_us(UPDATE_RATE_US, (ActivationFuncPtr)test_mode_update, NULL);

  SSBitmap bm[ROCKET6LINE_NUM_COLUMNS];
  char buf[ROCKET6LINE_NUM_COLUMNS + 2];
  static int test_state = 4;
  test_state++;
  if (test_state == 5) {
    test_state = 0;
  }
  switch (test_state) {
    case 0:
      strcpy(buf, "rrrrrrrr");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int row = 0; row < ROCKET6LINE_NUM_ROWS; row++) {
        rocket6line_write_line(row, bm, ROCKET6LINE_NUM_COLUMNS);
      }
      break;

    case 1:
      for (int row = 0; row < ROCKET6LINE_NUM_ROWS; row++) {
        sprintf(buf, "%d", row);
        ascii_to_bitmap_str(bm, 1, buf);
        for (int col = 0; col < ROCKET6LINE_NUM_COLUMNS; col++) {
          rocket6line_write_digit(row, col, bm[0]);
        }
      }
      break;

    case 2:
      strcpy(buf, "CCCCCCCC");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int row = 0; row < ROCKET6LINE_NUM_ROWS; row++) {
        rocket6line_write_line(row, bm, ROCKET6LINE_NUM_COLUMNS);
      }
      break;

    case 3:
      strcpy(buf, "01234567");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int row = 0; row < ROCKET6LINE_NUM_ROWS; row++) {
        rocket6line_write_line(row, bm, ROCKET6LINE_NUM_COLUMNS);
      }
      break;

    case 4:
      for (int row = 0; row < ROCKET6LINE_NUM_ROWS; row++) {
        for (int col = 0; col < ROCKET6LINE_NUM_COLUMNS; col++) {
          rocket6line_write_digit(row, col, 0xff);
        }
      }
      break;
  }
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif
  rocket6line_init();
  init_clock(10000, TIMER1);
  bss_canary_init();

  schedule_us(1, (ActivationFuncPtr)test_mode_update, NULL);

  cpumon_main_loop();
}
