#ifndef _GRAPHIC_LCD_12232_H
#define _GRAPHIC_LCD_12232_H

#include "rocket.h"

#define BITMAP_ROW_LEN	16	/* bytes; 128 pixels, six offscreen */
#define BITMAP_NUM_ROWS	32

typedef struct {
	Activation act;
	Activation *done_act;
	uint8_t framebuffer[BITMAP_NUM_ROWS][BITMAP_ROW_LEN];
} GLCD;

void glcd_init(GLCD *glcd, Activation *done_act);
//void glcd_clear_screen(GLCD *glcd);
void glcd_draw_framebuffer(GLCD *glcd);
void glcd_clear_framebuffer(GLCD *glcd);

uint8_t glcd_paint_char(GLCD *glcd, char glyph, uint8_t dx0);


#endif // _GRAPHIC_LCD_12232_H

