#ifndef __cursor_h__
#define __cursor_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#define MAX_HEIGHT 5

extern char cursor_label_white[0];

typedef struct s_cursor_act {
	ActivationFunc func;
	BoardBuffer bbuf[MAX_HEIGHT];
	uint8_t visible;
	RectRegion rr;
	uint8_t alpha;
	char *label;
} CursorAct;

void cursor_init(CursorAct *act);

void cursor_hide(CursorAct *act);
void cursor_show(CursorAct *act, RectRegion rr);
	// addresses are inclusive
void cursor_set_label(CursorAct *act, char *label);

#endif // _cursor_h
