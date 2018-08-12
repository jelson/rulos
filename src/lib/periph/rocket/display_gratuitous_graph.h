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

#ifndef display_gratuitous_graph_h
#define display_gratuitous_graph_h

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/rocket/drift_anim.h"

typedef struct s_d_gratuitous_graph {
	BoardBuffer bbuf;
	DriftAnim drift[3];
	char *name;
	Time impulse_frequency_us;
	Time last_impulse;
} DGratuitousGraph;

void dgg_init(DGratuitousGraph *dgg,
	uint8_t board, char *name, Time impulse_frequency_us);

#endif // display_gratuitous_graph_h
