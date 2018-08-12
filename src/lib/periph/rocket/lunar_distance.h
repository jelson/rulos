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

#ifndef _LUNAR_DISTANCE_H
#define _LUNAR_DISTANCE_H

#include "periph/rocket/rocket.h"
#include "periph/rocket/drift_anim.h"

typedef struct {
	DriftAnim da;
	BoardBuffer dist_board;
	BoardBuffer speed_board;
//	int adc_channel;
} LunarDistance;

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0, uint8_t speed_b0 /*, int adc_channel*/);
void lunar_distance_reset(LunarDistance *ld);
void lunar_distance_set_velocity_256ths(LunarDistance *ld, uint16_t frac);

#endif // _LUNAR_DISTANCE_H
