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

#include "periph/input_controller/input_controller.h"
#include "periph/rocket/rocket.h"

typedef struct s_autotype {
  InputInjectorIfc *iii;
  Time period;
  char *str;
  char *ptr;
} Autotype;

void init_autotype(Autotype *a, InputInjectorIfc *iii, char *str, Time period);
