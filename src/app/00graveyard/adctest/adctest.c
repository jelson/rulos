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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "periph/rocket/rocket.h"

/************************************************************************************/
/************************************************************************************/

typedef struct {
  ActivationFunc f;
  BoardBuffer bbuf;
  uint16_t *adc0;
  uint16_t *adc1;
} ADCAct_t;

static void update(ADCAct_t *a) {
  char buf[9];

  int_to_string2(buf, 4, 0, *a->adc0);
  int_to_string2(buf + 4, 4, 0, *a->adc1);
  buf[8] = '\0';

  ascii_to_bitmap_str(a->bbuf.buffer, 8, buf);
  board_buffer_draw(&a->bbuf);
  schedule_us(10000, (Activation *)a);
}

int main() {
  heap_init();
  util_init();
  rulos_hal_init();
  clock_init(10000);
  board_buffer_module_init();

  ADCAct_t a;
  a.f = (ActivationFunc)update;
  board_buffer_init(&a.bbuf);
  board_buffer_push(&a.bbuf, 7);
  a.adc0 = hal_get_adc(2);
  a.adc1 = hal_get_adc(3);

  schedule_us(1, (Activation *)&a);
  scheduler_run();
  return 0;
}
