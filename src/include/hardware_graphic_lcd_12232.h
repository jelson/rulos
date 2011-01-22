#ifndef _HARDWARE_GRAPHIC_LCD_12232_H
#define _HARDWARE_GRAPHIC_LCD_12232_H

#include "rocket.h"
#include "hardware.h"

typedef struct {
	Activation act;
	Activation *done_act;
} GLCD;

void glcd_init(GLCD *glcd, Activation *done_act);

void glcd_set_backlight(r_bool vbl);
uint16_t glcd_read_status(uint8_t bias);
void glcd_set_display_on(r_bool on);
void glcd_read_test();
void glcd_write_test();

void GLCD_Init(void);
void GLCD_SetPixel(unsigned char x, unsigned char y, unsigned char color);


#endif // _HARDWARE_GRAPHIC_LCD_12232_H
