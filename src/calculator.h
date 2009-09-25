#ifndef _calculator_h
#define _calculator_h

#include "focus.h"
#include "knob.h"
#include "numeric_input.h"
#include "cursor.h"
#include "board_buffer.h"
#include "display_scroll_msg.h"

typedef struct {
	BoardBuffer bbuf[2];
	BoardBuffer *btable[2];
	NumericInputAct operands[2];
	Knob operator;
	NumericInputAct result;
	CursorAct cursor;
	FocusManager focus;
} Calculator;

void calculator_init(Calculator *calc, int board0, FocusManager *fa);

#endif // _calculator_h
