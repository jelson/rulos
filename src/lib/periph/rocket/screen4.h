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

#ifndef _SCREEN4_H
#define _SCREEN4_H

#include "periph/7seg_panel/board_buffer.h"
#include "periph/rocket/rocket.h"

#define SCREEN4SIZE 4 /* shocking, I know */

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

#endif  // _SCREEN4_H
