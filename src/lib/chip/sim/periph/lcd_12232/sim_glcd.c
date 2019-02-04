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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/lcd_12232/graphic_lcd_12232.h"

/************* glcd ********************/

void glcd_init(GLCD *glcd, ActivationFuncPtr done_func, void *done_data) {}

void glcd_draw_framebuffer(GLCD *glcd) {}

void glcd_clear_framebuffer(GLCD *glcd) {}

uint8_t glcd_paint_char(GLCD *glcd, char glyph, int16_t dx0, r_bool invert) {
  return 0;
}
