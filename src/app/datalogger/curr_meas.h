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

#pragma once

#include <stdint.h>

#include "flash_dumper.h"

typedef struct {
  int addr;
  int channel_num;  // used for debug message only
  int scale;
  flash_dumper_t *flash_dumper;
  int loops_since_print;

  // number of times we've polled the current measurement module and gotten back
  // either "ready" or "not ready"
  int num_not_ready;
  int num_ready;

  // averaging: cumulative current and number of measurements
  int num_measurements;
  int32_t cum_current;
} currmeas_state_t;

void currmeas_init(currmeas_state_t *cms, uint8_t device_addr,
                   uint32_t prescale, uint16_t calibration, uint32_t scale,
                   uint32_t channel_num, flash_dumper_t *flash_dumper);
