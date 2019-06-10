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

#include "core/rulos.h"
#include "periph/audio/sound.h"

typedef struct {
  SoundToken token;
} SoundCmd;

typedef struct {
  uint8_t stream_id;
  r_bool skip;
  SoundCmd skip_cmd;
  SoundCmd loop_cmd;
} AudioRequestMessage;

typedef struct {
  uint8_t stream_id;
  uint8_t mlvolume;
} AudioVolumeMessage;

typedef struct {
  int8_t advance;  // +1: skip forward  -1: skip backward
} MusicControlMessage;
