#ifndef _SIM_H
#define _SIM_H

typedef struct s_board_layout {
	char *label;
	short colors[8];
	short x, y;
} BoardLayout;

void sim_display_light_status(r_bool status);

#define SIM_TWI_PORT_BASE 9470


#endif // _SIM_H
