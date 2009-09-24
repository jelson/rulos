#ifndef _cursor_h
#define _cursor_h

#include "board_buffer.h"
#include "clock.h"

#define MAX_HEIGHT 2

typedef struct s_cursor_act {
	ActivationFunc func;
	BoardBuffer bbuf[MAX_HEIGHT];
	uint8_t visible;
	uint8_t y0, y1, x0, x1;
} CursorAct;

void cursor_init(CursorAct *act);

void cursor_hide(CursorAct *act);
void cursor_show(CursorAct *act, uint8_t y0, uint8_t y1, uint8_t x0, uint8_t x1);
	// addresses are inclusive

#endif // _cursor_h
