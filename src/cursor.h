#ifndef _cursor_h
#define _cursor_h

#include "board_buffer.h"
#include "clock.h"

#define MAX_HEIGHT 5

typedef struct {
	uint8_t y0, y1, x0, x1;
} DisplayRect;

typedef struct s_cursor_act {
	ActivationFunc func;
	BoardBuffer bbuf[MAX_HEIGHT];
	uint8_t visible;
	DisplayRect rect;
	uint8_t alpha;
} CursorAct;

void cursor_init(CursorAct *act);

void cursor_hide(CursorAct *act);
void cursor_show(CursorAct *act, DisplayRect rect);
	// addresses are inclusive

#endif // _cursor_h
