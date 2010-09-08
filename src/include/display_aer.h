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

#ifndef display_aer_h
#define display_aer_h

#include "clock.h"
#include "board_buffer.h"
#include "drift_anim.h"
#include "calculator_decoration.h"

typedef struct s_d_aer {
	ActivationFunc func;
	BoardBuffer bbuf;
	DriftAnim azimuth;
	DriftAnim elevation;
	DriftAnim roll;
	Time impulse_frequency_us;
	Time last_impulse;

	struct s_decoration_ifc {
		FetchCalcDecorationValuesFunc func;
		struct s_d_aer *daer;
	} decoration_ifc;
} DAER;

void daer_init(DAER *daer, uint8_t board, Time impulse_frequency_us);

#endif // display_aer_h
