/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "chip/sim/core/sim.h"
#include "core/clock.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/rocket/rocket.h"
#include "periph/uart/serial_console.h"

#if !SIM
#include "core/hardware.h"
#endif  // !SIM

typedef struct {
  Activation act;
  uint8_t val;
  SerialConsole console;
} BlinkAct;

void _update_blink(Activation *act) {
  BlinkAct *ba = (BlinkAct *)act;
  ba->val = (ba->val << 1) | (ba->val >> 7);
#if !SIM
  PORTC = ba->val;
#endif

  serial_console_sync_send(&ba->console, "blink\n", 6);

  schedule_us(100000, &ba->act);
}

void blink_init(BlinkAct *ba) {
  ba->act.func = _update_blink;
  serial_console_init(&ba->console, NULL);
  serial_console_sync_send(&ba->console, "init\n", 5);
  ba->val = 0x0f;

#if !SIM
  gpio_make_output(GPIO_C0);
  gpio_make_output(GPIO_C1);
  gpio_make_output(GPIO_C2);
  gpio_make_output(GPIO_C3);
  gpio_make_output(GPIO_C4);
  gpio_make_output(GPIO_C5);
  gpio_make_output(GPIO_C6);
  gpio_make_output(GPIO_C7);
#endif  // !SIM

  schedule_us(100000, &ba->act);
}

int main() {
  util_init();
  hal_init(bc_audioboard);  // TODO need a "bc_custom"
  init_clock(1000, TIMER1);

  BlinkAct ba;
  blink_init(&ba);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase
  cpumon_main_loop();

  return 0;
}
