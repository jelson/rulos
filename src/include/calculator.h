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
