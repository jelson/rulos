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

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/rocket/calculator_decoration.h"
#include "periph/rocket/drift_anim.h"

struct s_d_aer;

typedef struct s_decoration_ifc {
  FetchCalcDecorationValuesFunc func;
  struct s_d_aer *daer;
} decoration_ifc_t;

typedef struct s_d_aer {
  BoardBuffer bbuf;
  DriftAnim azimuth;
  DriftAnim elevation;
  DriftAnim roll;
  Time impulse_frequency_us;
  Time last_impulse;
  decoration_ifc_t decoration_ifc;
} DisplayAzimuthElevationRoll;

void daer_init(DisplayAzimuthElevationRoll *daer, uint8_t board,
               Time impulse_frequency_us);
