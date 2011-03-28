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

#ifndef __cursor_h__
#define __cursor_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#define MAX_HEIGHT 5

extern char cursor_label_white[0];

typedef struct s_cursor_act {
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
