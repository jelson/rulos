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

#include "core/clock.h"
#include "core/network.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/rocket/thruster_protocol.h"

typedef struct s_d_thruster_graph {
  BoardBuffer bbuf;
  Network *network;
  uint8_t thruster_ring_storage[RECEIVE_RING_SIZE(1, sizeof(ThrusterPayload))];
  AppReceiver app_receiver;
  uint8_t thruster_bits;
  uint32_t value[4];
} DThrusterGraph;

void dtg_init_local(DThrusterGraph *dtg, uint8_t board);
void dtg_init_remote(DThrusterGraph *dtg, uint8_t board, Network *network);
void dtg_update_state(DThrusterGraph *dtg, ThrusterPayload *tp);
