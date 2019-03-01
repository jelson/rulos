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

#include "animate.h"
#include "core/rulos.h"
#include "frobutton.h"

/****************************************************************************/

#define SYSTEM_CLOCK 500

// UartState_t uart;

int main() {
  hal_init();
  init_clock(SYSTEM_CLOCK, TIMER0);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  //	uart_init(&uart, 38400, TRUE, 0);

  Animate an;
  animate_init(&an);
  //	animate_play(&an, Zap);

  Frobutton fb;
  frobutton_init(&fb, &an);

  cpumon_main_loop();

  return 0;
}
