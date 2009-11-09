#ifndef _thruster_protocol_h
#define _thruster_protocol_h

#include "network.h"
#include "network_ports.h"

#define THRUSTER_REAR		4
#define THRUSTER_FRONTLEFT	1
#define THRUSTER_FRONTRIGHT	2

#define THRUSTER_REPORT_INTERVAL 0500000

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
	ThrusterUpdateFunc func;
	Time last_send;
	Network *network;
	uint8_t thruster_message_storage[sizeof(Message)+sizeof(ThrusterPayload)];
	SendSlot sendSlot;
} ThrusterSendNetwork;

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network);

#endif // _thruster_protocol_h
