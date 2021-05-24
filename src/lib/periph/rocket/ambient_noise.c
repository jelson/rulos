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

#include "periph/rocket/ambient_noise.h"

#include "periph/audio/audio_client.h"

#define AMBIENT_NOISE_DECAY_PERIOD (30 * 1000000)

void ambient_noise_decay(AmbientNoise *an);

void ambient_noise_init(AmbientNoise *an, AudioClient *audio_client) {
  an->audio_client = audio_client;
  an->volume = VOL_MIN;

  ac_skip_to_clip(an->audio_client, AUDIO_STREAM_BACKGROUND,
                  sound_space_background, sound_space_background);

  schedule_us(AMBIENT_NOISE_DECAY_PERIOD,
              (ActivationFuncPtr)ambient_noise_decay, an);
}

void ambient_noise_boost_complete(AmbientNoise *an) {
  an->volume = VOL_DEFAULT;
  ac_skip_to_clip(an->audio_client, AUDIO_STREAM_BACKGROUND,
                  sound_space_background, sound_space_background);
  ac_change_volume(an->audio_client, AUDIO_STREAM_BACKGROUND, an->volume);
}

void ambient_noise_decay(AmbientNoise *an) {
  if (an->volume > VOL_MIN) {
    an->volume -= 1;  // quieter by one notch

    ac_change_volume(an->audio_client, AUDIO_STREAM_BACKGROUND, an->volume);
  }

  schedule_us(AMBIENT_NOISE_DECAY_PERIOD,
              (ActivationFuncPtr)ambient_noise_decay, an);
}
