#ifndef _calculator_h
#define _calculator_h

#include "focus.h"
#include "numeric_input.h"
#include "cursor.h"
#include "board_buffer.h"
#include "display_scroll_msg.h"

typedef struct {
	UIEventHandlerFunc func;
	BoardBuffer bbuf[2];
	NumericInputAct operands[2];
	Knob operator;
	NumericInputAct result;
	CursorAct cursor;
} Calculator;

void calculator_init(Calculator *calc, int board0, FocusAct *fa);

#endif // _calculator_h
