#pragma once

#include <rocket.h>

typedef enum e_movie { Idle, Zap } Movie;

struct s_cell;

typedef struct s_animate {
	uint8_t phase;
	uint8_t delay;
	struct s_cell *cell;
} Animate;

void animate_init(Animate* an);
void animate_play(Animate* an, Movie movie);
