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

#ifndef _display_blinker_h
#define _display_blinker_h

#include "periph/7seg_panel/board_buffer.h"
#include "core/clock.h"

typedef struct {
	uint16_t period;
	const char **msg;
	uint8_t cur_line;
	BoardBuffer bbuf;
} DBlinker;

void blinker_init(DBlinker *blinker, uint16_t period);
void blinker_set_msg(DBlinker *blinker, const char **msg);

#endif // _display_blinker_h
