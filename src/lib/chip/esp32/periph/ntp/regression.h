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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint64_t local_time_usec;
  uint64_t epoch_time_usec;
  uint32_t rtt_usec;
} time_observation_t;

static const uint16_t MIN_OBSERVATIONS = 10;
static const uint16_t MAX_OBSERVATIONS = 40;

bool update_epoch_estimate(const time_observation_t *obs, char *logbuf,
                           int *logbufcap, uint64_t *offset_usec /* OUT */,
                           int64_t *freq_ppb /* OUT */);
uint64_t local_to_epoch(uint64_t local_time_usec, uint64_t offset_usec,
                        int64_t freq_ppb);
