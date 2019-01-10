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

#ifndef _calculator_decoration_h
#define _calculator_decoration_h

#include "periph/rocket/numeric_input.h"
#include "periph/rocket/rocket.h"

struct s_decoration_state;
typedef void (*FetchCalcDecorationValuesFunc)(struct s_decoration_state *state,
                                              DecimalFloatingPoint *op0,
                                              DecimalFloatingPoint *op1);
typedef struct s_decoration_state {
  FetchCalcDecorationValuesFunc func;
} FetchCalcDecorationValuesIfc;

#endif  // _calculator_decoration_h
