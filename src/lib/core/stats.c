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

#include <string.h>

#include "core/rulos.h"
#include "core/stats.h"

void minmax_init(MinMaxMean_t *mmm) { memset(mmm, 0, sizeof(MinMaxMean_t)); }

void minmax_add_sample(MinMaxMean_t *mmm, const int32_t sample) {
  assert(sample >= 0);
  if (mmm->count == 0) {
    mmm->min = sample;
    mmm->max = sample;
    mmm->sum = sample;
  } else {
    mmm->min = min(mmm->min, sample);
    mmm->max = max(mmm->max, sample);
    mmm->sum += sample;
  }
  mmm->count++;
}

void minmax_log(MinMaxMean_t *mmm, const char *label) {
  LOG("%s stats: min=%ld, mean=%ld, max=%ld (n=%ld)", label, mmm->min,
      mmm->sum / mmm->count, mmm->max, mmm->count);
}
