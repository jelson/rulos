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

#define LIGHT_PIN GPIO_B3
#define TIMEOUT_MINUTES 30

/////////////////////////////

#include "chip/avr/periph/usi_serial/usi_serial.h"
#include "core/hardware.h"
#include "core/rulos.h"

uint16_t seconds_on = 0;

const uint16_t off_time = TIMEOUT_MINUTES * 60;
#define ONE_SEC_IN_USEC 1000000

static void one_sec(void *data) {
  usi_serial_send("S");

  if (seconds_on < off_time) {
    seconds_on++;
    schedule_us(ONE_SEC_IN_USEC, (ActivationFuncPtr)one_sec, NULL);
  } else {
    gpio_clr(LIGHT_PIN);
  }
}

int main() {
  hal_init();

  gpio_make_output(LIGHT_PIN);
  gpio_set(LIGHT_PIN);

#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);

  for (int i = 0; i < 20; i++) {
    gpio_set(TIMING_DEBUG_PIN);
    gpio_clr(TIMING_DEBUG_PIN);
  }
#endif

  usi_serial_send("A");

  init_clock(10000, TIMER0);
  schedule_now((ActivationFuncPtr)one_sec, NULL);

  usi_serial_send("L");

  cpumon_main_loop();
}
