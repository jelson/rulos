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

#ifndef _calculator_h
#define _calculator_h

#include "focus.h"
#include "knob.h"
#include "numeric_input.h"
#include "board_buffer.h"
#include "display_scroll_msg.h"
#include "calculator_decoration.h"

typedef struct s_calculator {
	UIEventHandlerFunc func;
	BoardBuffer bbuf[2];
	BoardBuffer *btable[2];
	NumericInputAct operands[2];
	Knob operator;
	NumericInputAct result;
	FocusManager focus;

	struct s_decoration_timeout {
		ActivationFunc func;
		struct s_calculator *calc;
		Time last_activity;
		FetchCalcDecorationValuesIfc *fetchDecorationValuesObj;
	} decorationTimeout;
} Calculator;

void calculator_init(
	Calculator *calc, int board0, FocusManager *fa,
	FetchCalcDecorationValuesIfc *fetchDecorationValuesObj);

#endif // _calculator_h
