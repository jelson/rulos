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
#include "periph/input_controller/input_controller.h"
#include "periph/quadknob/quadknob.h"
#include "periph/rocket/rocket.h"

#define DISPLAY_VOLUME_ADJUSTMENTS 1

// TODO drawing volume on the display burns 21 bytes of bss;
// here's where to reclaim them.

typedef struct {
  uint8_t mlvolume;
} SetVolumePayload;

struct s_volume_control;

typedef struct {
  InputInjectorIfc iii;
  struct s_volume_control *vc;
} VolumeControlInjector;

typedef struct s_volume_control {
#if DISPLAY_VOLUME_ADJUSTMENTS
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
  VolumeControlInjector injector;
  AudioClient *ac;

  uint8_t cur_vol;

#if DISPLAY_VOLUME_ADJUSTMENTS
  Time lastTouch;
  r_bool visible;
  uint8_t boardnum;
  BoardBuffer bbuf;
#endif  // DISPLAY_VOLUME_ADJUSTMENTS

  Keystroke vol_up;
  Keystroke vol_down;
} VolumeControl;

void volume_control_init(VolumeControl *vc, AudioClient *ac,
                         uint8_t boardnum,
                         Keystroke vol_up, Keystroke vol_down);

static inline uint8_t volume_get_mlvolume(VolumeControl *vc) {
  return vc->cur_vol;
}
