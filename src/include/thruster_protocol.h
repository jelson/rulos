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

#ifndef _thruster_protocol_h
#define _thruster_protocol_h

#define THRUSTER_REPORT_INTERVAL 250000

typedef struct {
	uint8_t thruster_bits;
} ThrusterPayload;

struct s_thruster_update;
typedef void (*ThrusterUpdateFunc)(
	struct s_thruster_update *tu,
	ThrusterPayload *payload);

typedef struct s_thruster_update
{
	ThrusterUpdateFunc func;
} ThrusterUpdate;

typedef struct {
	ActivationFunc func;
	struct s_thruster_send_network *tsn;
} ThrusterUpdateTimer;

typedef struct s_thruster_send_network {
	ThrusterUpdateFunc func;
	Network *network;
	uint8_t thruster_message_storage[sizeof(Message)+sizeof(ThrusterPayload)];
	SendSlot sendSlot;
	ThrusterPayload last_state;
	r_bool state_changed;
	ThrusterUpdateTimer timer;
} ThrusterSendNetwork;

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network);

#endif // _thruster_protocol_h
