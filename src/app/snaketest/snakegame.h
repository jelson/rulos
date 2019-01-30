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

#pragma once

#include "periph/audio/audio_client.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screen4.h"

#define CANVAS_W  16
#define CANVAS_H  12

typedef struct s_snake_handler {
  UIEventHandlerFunc func;
  struct s_snake *snake;
} SnakeHandler;

typedef struct s_point {
  uint8_t x, y;
} Point;

enum {
  RIGHT = 0,
  UP = 1,
  LEFT = 2,
  DOWN = 3,
  EMPTY = 4,
};
typedef uint8_t Direction;

typedef struct s_map {
  Direction cell[CANVAS_H][CANVAS_W];
} Map;

typedef struct s_snake {
  Screen4 *s4;
  SnakeHandler handler;
  AudioClient *audioClient;
  uint8_t score_boardnum;
  bool focused;
  BoardBuffer score_bbuf;

  Map map;
  Point head;
  Point tail;
  Direction direction;

  uint8_t ticks_per_grow;
  uint8_t grow_clock;
  uint16_t goal_length;
  uint16_t length;
} Snake;

void snake_init(Snake *snake, Screen4 *s4, AudioClient *audioClient, uint8_t score_boardnum);
