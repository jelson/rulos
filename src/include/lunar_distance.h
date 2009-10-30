#ifndef _LUNAR_DISTANCE_H
#define _LUNAR_DISTANCE_H

#include "rocket.h"
#include "drift_anim.h"

typedef struct {
	ActivationFunc func;
	DriftAnim da;
	BoardBuffer dist_board;
	BoardBuffer speed_board;
} LunarDistance;

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0, uint8_t speed_b0);

#endif // _LUNAR_DISTANCE_H
