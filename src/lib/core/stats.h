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

#include <inttypes.h>

#include "core/util.h"

typedef struct {
  int32_t min;
  int32_t max;
  int32_t sum;
  uint32_t count;
} MinMaxMean_t;

void minmax_init(MinMaxMean_t *mmm);
void minmax_add_sample(MinMaxMean_t *mmm, int32_t sample);
void minmax_log(MinMaxMean_t *mmm, const char *label);
