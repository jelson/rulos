#ifndef focus_h
#define focus_h

#include "clock.h"
#include "board_buffer.h"
#include "input_controller.h"

struct s_focus_act;

typedef struct {
	InputHandlerFunc func;
	struct s_focus_act *act;
		// TODO if we're desperately starving for RAM, we could compute
		// &FocusAct from &act->inputHandler by subtraction (shudder),
		// saving this pointer.
} FocusInputHandler;

typedef struct s_focus_act {
	ActivationFunc func;
	BoardBuffer board_buffer;
	uint8_t board;
	FocusInputHandler inputHandler;
	InputHandler *childHandler;
	uint32_t selectUntilTime;
} FocusAct;

void focus_init(FocusAct *act);

#endif // focus_h

