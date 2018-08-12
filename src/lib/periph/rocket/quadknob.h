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

#ifndef _QUADKNOB_H
#define _QUADKNOB_H

#include "rocket.h"
#include "input_controller.h"

#ifndef SIM
#include "hardware.h"
#endif // SIM

typedef struct s_quadknob {
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
