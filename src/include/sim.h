#ifndef _SIM_H
#define _SIM_H

typedef struct s_board_layout {
	char *label;
	short colors[8];
	short x, y;
} BoardLayout;

extern BoardLayout *tree0, *tree1, *tree2, *wallclock_tree;
void sim_configure_tree(BoardLayout *tree);

typedef struct {
	r_bool initted;
	int instance;
	Activation *user_act;
	int udp_socket;
	uint8_t read_byte;
	r_bool read_ready;
} TWIState;

void sim_twi_set_instance(int instance);
void twi_poll();

#define SIM_TWI_PORT_BASE 9470
#define SIM_TWI_NUM_NODES 4

#endif // _SIM_H
