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

#ifndef display_thruster_graph_h
#define display_thruster_graph_h

#include "clock.h"
#include "board_buffer.h"
#include "thruster_protocol.h"
#include "network.h"

typedef struct s_d_thruster_graph {
	ActivationFunc func;
	BoardBuffer bbuf;
	Network *network;
	uint8_t thruster_message_storage[sizeof(Message)+sizeof(ThrusterPayload)];
	RecvSlot recvSlot;
	uint8_t thruster_bits;
	uint32_t value[4];
} DThrusterGraph;

void dtg_init(DThrusterGraph *dtg, uint8_t board, Network *network);

#endif // display_thruster_graph_h
