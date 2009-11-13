#ifndef _thruster_protocol_h
#define _thruster_protocol_h

#include "network.h"
#include "network_ports.h"

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
