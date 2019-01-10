/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "periph/rocket/ambient_noise.h"
#include "periph/audio/audio_client.h"

#define AMBIENT_NOISE_DECAY_PERIOD (120 * 1000000)

void ambient_noise_decay(AmbientNoise *an);

void ambient_noise_init(AmbientNoise *an, AudioClient *audio_client) {
  an->audio_client = audio_client;
  an->mlvolume = 8;

  ac_skip_to_clip(an->audio_client, AUDIO_STREAM_BACKGROUND,
                  sound_space_background, sound_space_background);

  schedule_us(AMBIENT_NOISE_DECAY_PERIOD,
              (ActivationFuncPtr)ambient_noise_decay, an);
}

void ambient_noise_boost_complete(AmbientNoise *an) {
  an->mlvolume = 1;
  ac_skip_to_clip(an->audio_client, AUDIO_STREAM_BACKGROUND,
                  sound_space_background, sound_space_background);
  ac_change_volume(an->audio_client, AUDIO_STREAM_BACKGROUND, an->mlvolume);
}

void ambient_noise_decay(AmbientNoise *an) {
  if (an->mlvolume < 8) {
    an->mlvolume += 1;  // quieter by one notch

    ac_change_volume(an->audio_client, AUDIO_STREAM_BACKGROUND, an->mlvolume);
  }

  schedule_us(AMBIENT_NOISE_DECAY_PERIOD,
              (ActivationFuncPtr)ambient_noise_decay, an);
}
