#include "joystick.h"

typedef struct {
	ActivationFunc func;
	JoystickState_t joystick_state;
	BoardBuffer bbuf;
} ThrusterState_t;

void thrusters_init(ThrusterState_t *ts,
					uint8_t board,
					uint8_t x_chan,
					uint8_t y_chan);

