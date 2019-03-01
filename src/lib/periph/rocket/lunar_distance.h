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

#include "periph/rocket/drift_anim.h"
#include "periph/rocket/rocket.h"

typedef struct {
  DriftAnim da;
  BoardBuffer dist_board;
  BoardBuffer speed_board;
  //	int adc_channel;
} LunarDistance;

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0,
                         uint8_t speed_b0 /*, int adc_channel*/);
void lunar_distance_reset(LunarDistance *ld);
void lunar_distance_set_velocity_256ths(LunarDistance *ld, uint16_t frac);
