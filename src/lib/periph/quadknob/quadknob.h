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

#pragma once

#include "core/clock.h"
#include "periph/input_controller/input_controller.h"
//#include "periph/rocket/rocket.h"

#ifndef SIM
#include "core/hardware.h"
#endif  // SIM

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

void init_quadknob(QuadKnob *qk, InputInjectorIfc *ifi,
#ifndef SIM
                   IOPinDef *pin0, IOPinDef *pin1,
#endif
                   char fwd, char back);
