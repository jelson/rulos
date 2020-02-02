/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "periph/audio/audio_client.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screen4.h"

#define CANVAS_W 16
#define CANVAS_H (SCREEN4SIZE * 3)

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

enum {
  PLAYING = 0,
  EXPLODING = 1,
  GAME_OVER = 2,
};
typedef uint8_t Mode;

typedef struct s_snake {
  Screen4 *s4;
  SnakeHandler handler;
  AudioClient *audioClient;
  bool focused;
  uint8_t score_boardnum;
  BoardBuffer score_bbuf;
  uint8_t status_boardnum;
  BoardBuffer status_bbuf;

  Map map;
  Point head;
  Point tail;
  Direction direction;

  uint8_t move_clock;
  uint8_t grow_clock;
  uint16_t goal_length;
  uint16_t length;
  Mode mode;
  uint8_t explosion_radius;  // during game-over animation
} Snake;

void snake_init(Snake *snake, Screen4 *s4, AudioClient *audioClient,
                uint8_t score_boardnum, uint8_t status_boardnum);
