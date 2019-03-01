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

#include "periph/rocket/numeric_input.h"
#include "periph/rocket/rocket.h"

struct s_decoration_state;
typedef void (*FetchCalcDecorationValuesFunc)(struct s_decoration_state *state,
                                              DecimalFloatingPoint *op0,
                                              DecimalFloatingPoint *op1);
typedef struct s_decoration_state {
  FetchCalcDecorationValuesFunc func;
} FetchCalcDecorationValuesIfc;
