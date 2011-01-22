#ifndef _HARDWARE_GRAPHIC_LCD_12232_H
#define _HARDWARE_GRAPHIC_LCD_12232_H

#include "rocket.h"
#include "hardware.h"

typedef struct {
} GLCD;

void glcd_init();
void glcd_set_backlight(r_bool vbl);
uint16_t glcd_read_status(uint8_t bias);
void glcd_set_display_on(r_bool on);
void glcd_read_test();
void glcd_write_test();

#endif // _HARDWARE_GRAPHIC_LCD_12232_H
