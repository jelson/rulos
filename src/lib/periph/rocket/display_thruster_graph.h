/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

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
