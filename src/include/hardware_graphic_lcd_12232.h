#ifndef _HARDWARE_GRAPHIC_LCD_12232_H
#define _HARDWARE_GRAPHIC_LCD_12232_H

#include "rocket.h"
#include "hardware.h"

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


void glcd_set_backlight(r_bool vbl);
uint16_t glcd_read_status(uint8_t bias);
void glcd_set_display_on(r_bool on);
void glcd_read_test();
void glcd_write_test();



#endif // _HARDWARE_GRAPHIC_LCD_12232_H
