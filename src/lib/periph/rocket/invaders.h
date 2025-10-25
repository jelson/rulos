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

#define INV_CANVAS_W       30
#define INV_CANVAS_H       (SCREEN4SIZE * 4)
#define INV_ALIEN_ROWS     5
#define INV_ALIEN_COLS     6
#define INV_MAX_ALIENS     (INV_ALIEN_ROWS * INV_ALIEN_COLS)
#define INV_MAX_PLAYER_BULLETS 2
#define INV_MAX_ALIEN_BULLETS  3
#define INV_SHIP_W         3
#define INV_SHIP_H         2
#define INV_ALIEN_W        2
#define INV_ALIEN_H        2

typedef struct s_invaders_handler {
  UIEventHandlerFunc func;
  struct s_invaders *invaders;
} InvadersHandler;

typedef struct {
  int8_t x;
  int8_t y;
  bool active;
} Bullet;

typedef struct {
  uint8_t x;  // x position of the alien formation (upper-left corner)
  uint8_t y;  // y position of the alien formation (upper-left corner)
  bool alive[INV_MAX_ALIENS];  // which aliens are still alive
  int8_t dx;  // direction of movement: 1=right, -1=left
  uint8_t move_clock;
  uint8_t shoot_clock;
} AlienFormation;

enum {
  INV_PLAYING = 0,
  INV_PLAYER_EXPLODING = 1,
  INV_GAME_OVER = 2,
  INV_VICTORY = 3,
};
typedef uint8_t InvadersMode;

typedef struct s_invaders {
  Screen4 *s4;
  InvadersHandler handler;
  AudioClient *audioClient;
  JoystickState_t *joystick;
  bool focused;

  uint8_t lives_boardnum;
  BoardBuffer lives_bbuf;
  uint8_t score_boardnum;
  BoardBuffer score_bbuf;

  // Game state
  InvadersMode mode;
  int8_t ship_x;  // x position of player ship
  uint16_t score;
  uint8_t lives;

  // Bullets
  Bullet player_bullets[INV_MAX_PLAYER_BULLETS];
  Bullet alien_bullets[INV_MAX_ALIEN_BULLETS];

  // Aliens
  AlienFormation aliens;
  uint8_t aliens_remaining;

  // Animation
  uint8_t explosion_radius;
  int8_t explosion_x;
  int8_t explosion_y;

  // Input tracking
  bool last_trigger_state;
} Invaders;

void invaders_init(Invaders *inv, Screen4 *s4, AudioClient *audioClient,
                   JoystickState_t *joystick, uint8_t lives_boardnum,
                   uint8_t score_boardnum);
