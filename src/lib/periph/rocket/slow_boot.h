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
#include "periph/rocket/screenblanker.h"

#define SLOW_MAX_BUFFERS NUM_TOTAL_BOARDS

//#define BORROW_SCREENBLANKER_BUFS 1

typedef struct s_slow_boot {
  ScreenBlanker *screenblanker;
#if BORROW_SCREENBLANKER_BUFS
#error THIS CODE IS A BAD IDEA. DO NOT ACTIVATE
  BoardBuffer *buffer;
#else   // BORROW_SCREENBLANKER_BUFS
  BoardBuffer buffer[SLOW_MAX_BUFFERS];
#endif  // BORROW_SCREENBLANKER_BUFS
  Time startTime;
  AudioClient *audioClient;
} SlowBoot;

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker,
                    AudioClient *audioClient);
