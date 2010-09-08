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

#ifndef display_rtc_h
#define display_rtc_h

#include "clock.h"
#include "board_buffer.h"

typedef struct {
	ActivationFunc func;
	Time base_time;
	BoardBuffer bbuf;
} DRTCAct;

void drtc_init(DRTCAct *act, uint8_t board, Time base_time);
void drtc_set_base_time(DRTCAct *act, Time base_time);
Time drtc_get_base_time(DRTCAct *act);

#endif // display_rtc_h
