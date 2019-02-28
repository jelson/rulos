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

#pragma once

#include <assert.h>

#include "core/hal.h"
#include "core/util.h"

typedef struct s_board_layout {
  const char *label;
  short colors[8];
  short x, y;
} BoardLayout;

void sim_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void sim_display_light_status(r_bool status);

void sim_register_clock_handler(Handler func, void *data);
void sim_register_sigio_handler(Handler func, void *data);

#define SIM_TWI_PORT_BASE 9470
