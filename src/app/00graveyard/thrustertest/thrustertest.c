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

#include "periph/joystick/joystick.h"
#include "periph/rocket/display_thrusters.h"
#include "periph/rocket/rocket.h"

/************************************************************************************/
/************************************************************************************/

int main() {
  HPAM hpam;
  ThrusterUpdate *thrusterUpdate[4];

  util_init();
  rulos_hal_init(bc_rocket0);
  init_clock(10000, TIMER1);
  memset(thrusterUpdate, 0, sizeof(thrusterUpdate));
  init_hpam(&hpam, 7, thrusterUpdate);
  board_buffer_module_init();

  ThrusterState_t ts;
  thrusters_init(&ts, 7, 3, 2, &hpam, NULL);

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();
  return 0;
}
