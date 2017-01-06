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

#ifndef _SIM_H
#define _SIM_H

typedef struct s_board_layout {
	const char *label;
	short colors[8];
	short x, y;
} BoardLayout;

void sim_display_light_status(r_bool status);

void sim_register_clock_handler(Handler func, void *data);
void sim_register_sigio_handler(Handler func, void *data);

#define SIM_TWI_PORT_BASE 9470

#endif // _SIM_H
