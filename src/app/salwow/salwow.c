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

#include "control.h"
#include "core/clock.h"
#include "core/rulos.h"
#include "core/util.h"
#include "leds.h"
#include "mark_point.h"
#include "periph/uart/uart.h"

/****************************************************************************/

#define SYSTEM_CLOCK 500

//////////////// UART ///////////////////////////////////////

UartState_t log_uart;
const char *_test_msg[2] = {"Aa", "Bb"};
char **test_msg = (char **)_test_msg;

void send_done(void *data);

void send_one(void *data) {
  static int ctr;
  mark_point(16 + ((ctr++) & 15));

  LOG("done %d", ctr);
}

void send_done(void *data) {
  schedule_us(1, send_one, data);
}

int main() {
  mark_point_init();
  mark_point(1);
  rulos_hal_init();
  mark_point(2);
  init_clock(SYSTEM_CLOCK, TIMER0);

  leds_init();

  mark_point(3);
  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  mark_point(4);
  uart_init(&log_uart, 0, 38400);
  log_bind_uart(&log_uart);
  LOG("SALWOW up.");

#if 0
	uint8_t f = 0;
	while(1) {
		f+=1;
		mark_point((f&1) | 16);
		LOG("my dog. worms.");
	}
#endif

#if 0
	GPSInput gpsi;
	gpsinput_init(&gpsi, 1, _test_sentence_done, &gpsi);
#endif

#if 0
	RudderState rudder;
	rudder_init(&rudder);
	rudder_test_mode(&rudder);

	MotorState motors;
	motors_init(&motors);
	motors_test_mode(&motors);

	mathtest();
#endif

  Vector wpts[] = {
      {-122.341817, 47.672009},
      {-122.341966, 47.672792},
      {-122.342680, 47.672989},
      {-122.342921, 47.672284},
  };

  Control control;
  control_init(&control);
  for (uint16_t i = 0; i < (sizeof(wpts) / sizeof(wpts[0])); i++) {
    control_add_waypoint(&control, &wpts[i]);
  }

#if 1
  control_start(&control);
#else
  control_test_rudder(&control);
#endif

  cpumon_main_loop();
  mark_point(7);

  return 0;
}
