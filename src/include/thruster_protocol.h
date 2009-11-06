#ifndef _thruster_protocol_h
#define _thruster_protocol_h

typedef struct {
	uint8_t thruster_bits;
} ThrusterPayload;
#define THRUSTER_PORT (0x11)

#endif // _thruster_protocol_h
