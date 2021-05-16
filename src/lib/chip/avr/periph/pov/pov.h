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

#include "periph/rocket/rocket.h"

#ifndef SIM
#include "core/hardware.h"
#endif  // SIM

#define POV_LG_DISPLAY_WIDTH 5
#define POV_DISPLAY_WIDTH (1 << POV_LG_DISPLAY_WIDTH)

#define POVLEDA GPIO_B4
#define POVLEDB GPIO_B5
#define POVLEDC GPIO_B3
#define POVLEDD GPIO_D6
#define POVLEDE GPIO_B0

typedef struct {
  // written by measurement func; read by display func
  bool last_wave_positive;
  Time lastPhase;
  Time curPhase;
  Time lastPeriod;
  uint16_t debug_position;
  char debug_display_on;
  char debug_reverse;

  bool visible;

  uint8_t message[POV_DISPLAY_WIDTH];
} PovAct;

void pov_init(PovAct *povAct);
void pov_measure(PovAct *povAct);
void pov_display(PovAct *povAct);
void pov_write(PovAct *povAct, char *msg);
void pov_set_visible(PovAct *povAct, bool visible);

void pov_paint(uint8_t bitmap);
