#ifndef _SCREEN4_H
#define _SCREEN4_H

#include "rocket.h"
#include "board_buffer.h"

#define SCREEN4SIZE		4	/* shocking, I know */

typedef struct {
	uint8_t board0;
	BoardBuffer bbuf[SCREEN4SIZE];
	BoardBuffer *bbufp[SCREEN4SIZE];
	RectRegion rrect;
} Screen4;

void init_screen4(Screen4 *s4, uint8_t board0);
void s4_show(Screen4 *s4);
void s4_hide(Screen4 *s4);
void s4_draw(Screen4 *s4);
r_bool s4_visible(Screen4 *s4);

#endif // _SCREEN4_H
