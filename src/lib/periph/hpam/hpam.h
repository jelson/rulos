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
#include "core/network.h"
#include "core/util.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/rocket/thruster_protocol.h"

typedef struct {
  bool status;
  uint8_t rest_time_secs;
  Time expire_time;  // time when needs to rest, or when rest complete
  bool resting;
  uint8_t board;
  uint8_t digit;
  uint8_t segment;
} HPAMPort;

typedef enum {
  HPAM_HOBBS = 0,
  HPAM_RESERVED = 1,  // future: clanger, hatch open solenoid
  HPAM_LIGHTING_FLICKER = 2,
  HPAM_FIVE_VOLTS = 3,  // 5V channel, no longer used
  HPAM_THRUSTER_FRONTLEFT = 4,
  HPAM_THRUSTER_FRONTRIGHT = 5,
  HPAM_THRUSTER_REAR = 6,
  HPAM_BOOSTER = 7,
  HPAM_END
} HPAMIndex;

// Future: gauges on a 12V HPAM.

typedef struct {
  HPAMPort hpam_ports[HPAM_END];
  ThrusterUpdate *thrusterUpdates;
  BoardBuffer bbuf;
  ThrusterPayload thrusterPayload;
} HPAM;

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate *thrusterUpdates);
void hpam_set_port(HPAM *hpam, HPAMIndex idx, bool status);
bool hpam_get_port(HPAM *hpam, HPAMIndex idx);
