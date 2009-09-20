#ifndef focus_h
#define focus_h

#include "clock.h"
#include "board_buffer.h"

typedef struct {
	ActivationFunc func;
	BoardBuffer board_buffer;
	uint8_t board;
} FocusAct;

void focus_init(FocusAct *act);

#endif // focus_h

