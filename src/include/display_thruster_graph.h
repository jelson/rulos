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
	struct s_d_thruster_graph *self;
	uint8_t thruster_bits;
} DThrusterGraph;

void dtg_init(DThrusterGraph *dtg, uint8_t board, Network *network);

#endif // display_thruster_graph_h
