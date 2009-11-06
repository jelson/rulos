#include "joystick.h"
#include "network.h"
#include "thruster_protocol.h"

typedef struct {
	ActivationFunc func;
	JoystickState_t joystick_state;
	BoardBuffer bbuf;
	Network *network;
	uint8_t thruster_message_storage[sizeof(Message)+sizeof(ThrusterPayload)];
	SendSlot sendSlot;
	Time last_send;
} ThrusterState_t;

#define THRUSTER_REPORT_INTERVAL 0500000

void thrusters_init(ThrusterState_t *ts,
					uint8_t board,
					uint8_t x_chan,
					uint8_t y_chan,
					Network *network);

