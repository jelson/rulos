#pragma once

#include <stdint.h>

typedef enum e_movie { Idle, Zap, Diddle } Movie;

struct s_cell;

typedef struct s_animate {
  uint8_t phase;
  uint16_t delay;
  struct s_cell* cell;
} Animate;

void animate_init(Animate* an);
void animate_play(Animate* an, Movie movie);
