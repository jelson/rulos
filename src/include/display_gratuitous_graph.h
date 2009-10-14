#ifndef display_gratuitous_graph_h
#define display_gratuitous_graph_h

#include "clock.h"
#include "board_buffer.h"
#include "drift_anim.h"

typedef struct s_d_gratuitous_graph {
	ActivationFunc func;
	BoardBuffer bbuf;
	DriftAnim drift;
	char *name;
	Time impulse_frequency_us;
	Time last_impulse;
} DGratuitousGraph;

void dgg_init(DGratuitousGraph *dgg,
	uint8_t board, char *name, Time impulse_frequency_us);

#endif // display_gratuitous_graph_h
