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

#ifndef display_compass_h
#define display_compass_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"
#include "drift_anim.h"

struct s_dcompassact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_dcompassact *act;
} DCompassHandler;

typedef struct s_dcompassact {
	BoardBuffer bbuf;
	BoardBuffer *btable;	// needed for RectRegion
	DCompassHandler handler;
	uint8_t focused;
	DriftAnim drift;
	uint32_t last_impulse_time;
} DCompassAct;

void dcompass_init(DCompassAct *act, uint8_t board, FocusManager *focus);

#endif // display_compass_h
