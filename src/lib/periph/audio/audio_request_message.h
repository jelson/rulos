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
  // The sound stream on which to play different effect(s).
  uint8_t stream_idx;

  // If skip is true, immediately begin playing skip_effect_id, then loop
  // loop_effect_id If skip is false, allow existing effect to complete, then
  // loop loop_effect_id
  bool skip;
  SoundEffectId skip_effect_id:8;
  SoundEffectId loop_effect_id:8;
  uint8_t volume;
} AudioRequestMessage;

typedef struct {
  // The sound stream whose volume should change.
  uint8_t stream_idx;

  // The new volume [ VOL_MIN, VOL_MAX ]
  // TODO: someday a gorgeous finer-grained log volume scale.
  uint8_t volume;
} AudioVolumeMessage;

typedef struct {
  // +1: skip forward  -1: skip backward
  int8_t advance;
  uint8_t volume;
} MusicControlMessage;
