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
#include "periph/joystick/joystick.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screen4.h"

#define BRK_CANVAS_W     30
#define BRK_CANVAS_H     (SCREEN4SIZE * 4)
#define BRK_PADDLE_W     6
#define BRK_PADDLE_H     2
#define BRK_BALL_SIZE    2
#define BRK_BRICK_W      2
#define BRK_BRICK_H      2
#define BRK_BRICK_ROWS   6
#define BRK_BRICK_COLS   10
#define BRK_MAX_BRICKS   (BRK_BRICK_ROWS * BRK_BRICK_COLS)

// Scaled coordinates for sub-pixel precision
#define BRK_SCALE2       6
#define BRK_SCALE        (1 << BRK_SCALE2)

typedef struct s_breakout_handler {
  UIEventHandlerFunc func;
  struct s_breakout *breakout;
} BreakoutHandler;

enum {
  BRK_WAITING = 0,   // Ball stuck to paddle, waiting for launch
  BRK_PLAYING = 1,   // Ball in motion
  BRK_DYING = 2,     // Ball lost, explosion animation
  BRK_GAME_OVER = 3, // All lives lost
  BRK_VICTORY = 4,   // All bricks destroyed
};
typedef uint8_t BreakoutMode;

typedef struct s_breakout {
  Screen4 *s4;
  BreakoutHandler handler;
  AudioClient *audioClient;
  JoystickState_t *joystick;
  bool focused;

  uint8_t lives_boardnum;
  BoardBuffer lives_bbuf;
  uint8_t score_boardnum;
  BoardBuffer score_bbuf;

  // Game state
  BreakoutMode mode;
  uint8_t lives;
  uint16_t score;

  // Paddle
  int8_t paddle_x;  // x position of paddle (unscaled)

  // Ball (using scaled coordinates for smooth movement)
  int16_t ball_x;  // scaled by BRK_SCALE
  int16_t ball_y;  // scaled by BRK_SCALE
  int16_t ball_dx; // scaled velocity
  int16_t ball_dy; // scaled velocity

  // Bricks
  bool bricks[BRK_MAX_BRICKS];
  uint8_t bricks_remaining;

  // Animation
  uint8_t explosion_radius;
  int8_t explosion_x;
  int8_t explosion_y;

  // Input tracking
  bool last_trigger_state;
} Breakout;

void breakout_init(Breakout *brk, Screen4 *s4, AudioClient *audioClient,
                   JoystickState_t *joystick, uint8_t lives_boardnum,
                   uint8_t score_boardnum);
