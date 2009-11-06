#ifndef display_aer_h
#define display_aer_h

#include "clock.h"
#include "board_buffer.h"
#include "drift_anim.h"

typedef struct s_d_aer {
	ActivationFunc func;
	BoardBuffer bbuf;
	DriftAnim azimuth;
	DriftAnim elevation;
	DriftAnim roll;
	Time impulse_frequency_us;
	Time last_impulse;
} DAER;

void daer_init(DAER *daer, uint8_t board, Time impulse_frequency_us);

#endif // display_aer_h