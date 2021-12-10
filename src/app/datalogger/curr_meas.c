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

#include "curr_meas.h"

#include "core/rulos.h"
#include "flash_dumper.h"
#include "ina219.h"

#define PRINT_INTERVAL_USEC        1000000
#define POWERMEASURE_POLLTIME_USEC 20000

static void measure_current(currmeas_state_t *cms) {
  static int16_t current;
  bool is_ready = ina219_read_microamps(cms->addr, &current);

  if (!is_ready) {
    cms->num_not_ready++;
  } else {
    cms->num_ready++;
    cms->cum_current += current;
    cms->num_measurements++;
  }
}

static void print_current(currmeas_state_t *cms) {
  char msg[100];
  int len = snprintf(msg, sizeof(msg), "curr,%d,%ld", cms->channel_num,
                     cms->scale * cms->cum_current / cms->num_measurements);
  flash_dumper_write(cms->flash_dumper, msg, len);
  cms->cum_current = 0;
  cms->num_measurements = 0;

  uint32_t usec =
      POWERMEASURE_POLLTIME_USEC * (cms->num_ready + cms->num_not_ready);
  uint32_t ups = usec / cms->num_ready;
  LOG("chan %d current: %d samples ready, %d not; avg %ld usec per sample",
      cms->channel_num, cms->num_ready, cms->num_not_ready, ups);
}

// current measurement
static void monitor_current(void *data) {
  schedule_us(POWERMEASURE_POLLTIME_USEC, monitor_current, data);
  currmeas_state_t *cms = (currmeas_state_t *)data;
  cms->loops_since_print++;

  measure_current(cms);

  if (cms->loops_since_print ==
      (PRINT_INTERVAL_USEC / POWERMEASURE_POLLTIME_USEC)) {
    cms->loops_since_print = 0;
    print_current(cms);
  }
}

void currmeas_init(currmeas_state_t *cms, uint8_t device_addr,
                   uint32_t prescale, uint16_t calibration, uint32_t scale,
                   uint32_t channel_num, flash_dumper_t *flash_dumper) {
  memset(cms, 0, sizeof(*cms));
  cms->addr = device_addr;
  cms->scale = scale;
  cms->channel_num = channel_num;
  cms->flash_dumper = flash_dumper;

  if (ina219_init(device_addr, prescale, calibration)) {
    schedule_now(monitor_current, cms);
  }
}
