/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#define PONG_HEIGHT 4
#define PONG_SCALE2 6
#define PONG_SCALE (1 << PONG_SCALE2)
#define PONG_FREQ2 5
#define PONG_FREQ (1 << PONG_FREQ2)

typedef struct s_pong_handler {
  UIEventHandlerFunc func;
  struct s_pong *pong;
} PongHandler;

typedef struct s_pong {
  //	BoardBuffer bbuf[PONG_HEIGHT];
  //	BoardBuffer *btable[PONG_HEIGHT];
  //	RectRegion rrect;
  Screen4 *s4;
  PongHandler handler;
  int x, y, dx, dy;  // scaled by PONG_SCALE
  int paddley[2];
  int score[2];
  Time lastScore;
  r_bool focused;
  AudioClient *audioClient;
} Pong;

void pong_init(Pong *pong, Screen4 *s4, AudioClient *audioClient);
