#ifndef _QUADKNOB_H
#define _QUADKNOB_H

#include "rocket.h"
#include "input_controller.h"

#ifndef SIM
#include "hardware.h"
#endif // SIM

typedef struct s_quadknob {
	ActivationFunc func;
	uint8_t oldState;
	InputInjectorIfc *ifi;

#ifndef SIM
	IOPinDef *pin0;
	IOPinDef *pin1;
#endif
	char fwd;
	char back;
} QuadKnob;

void init_quadknob(QuadKnob *qk,
	InputInjectorIfc *ifi,
#ifndef SIM
	IOPinDef *pin0,
	IOPinDef *pin1,
#endif
	char fwd,
	char back);

#endif // _QUADKNOB_H
