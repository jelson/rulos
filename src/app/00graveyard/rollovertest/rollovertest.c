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
  ActivationFunc update;
  int task;
  int interval;
} RolloverTest_t;

static void update(RolloverTest_t *rt) {
  LOG("task %d running at jiffy %d", rt->task, clock_time_us());
  schedule_us(rt->interval, (Activation *)rt);
}

int main() {
  heap_init();
  util_init();
  rulos_hal_init();
  clock_init(1000);

  RolloverTest_t t1, t2, t3, t4;
  t1.update = t2.update = t3.update = t4.update = (ActivationFunc)update;
  t1.task = 1;
  t1.interval = 10000;

  t2.task = 2;
  t2.interval = 5000;

  t3.task = 3;
  t3.interval = 6000;

  t4.task = 4;
  t4.interval = 3000;

  schedule_us(1, (Activation *)&t1);
  schedule_us(1, (Activation *)&t2);
  //	schedule_us(1, (Activation *) &t3);
  //	schedule_us(1, (Activation *) &t4);

  scheduler_run();
  return 0;
}
