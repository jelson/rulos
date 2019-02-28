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
#include "periph/hpam/hpam.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screenblanker.h"

typedef enum { bcontext_liftoff, bcontext_docking } BoosterContext;

typedef struct {
  r_bool status;
  HPAM *hpam;
  AudioClient *audioClient;
  ScreenBlanker *screenblanker;
  BoosterContext bcontext;
} Booster;

void booster_init(Booster *booster, HPAM *hpam, AudioClient *audioClient,
                  ScreenBlanker *screenblanker);
void booster_set_context(Booster *booster, BoosterContext bcontext);
void booster_set(Booster *booster, r_bool status);
