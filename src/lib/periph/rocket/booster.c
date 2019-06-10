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

#include "periph/rocket/booster.h"

void booster_init(Booster *booster, HPAM *hpam, AudioClient *audioClient,
                  ScreenBlanker *screenblanker) {
  booster->status = FALSE;
  booster->hpam = hpam;
  booster->audioClient = audioClient;
  booster->screenblanker = screenblanker;
  booster->bcontext = bcontext_liftoff;
  hpam_set_port(booster->hpam, HPAM_BOOSTER, FALSE);
  ambient_noise_init(&booster->ambient_noise, audioClient);
}

void booster_set_context(Booster *booster, BoosterContext bcontext) {
  booster->bcontext = bcontext;
}

void booster_set(Booster *booster, r_bool status) {
  if (status == booster->status) {
    return;
  }

  if (status) {
    booster->status = TRUE;
    hpam_set_port(booster->hpam, HPAM_BOOSTER, TRUE);
    ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_booster_start, sound_booster_running);
    screenblanker_setmode(booster->screenblanker, sb_flicker);
  } else {
    booster->status = FALSE;
    hpam_set_port(booster->hpam, HPAM_BOOSTER, FALSE);

    if (booster->bcontext == bcontext_liftoff) {
      ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                      sound_booster_flameout_cutoff, sound_silence);
      // goofball hack to deal with output buffer queue limit (1):
      // Get the booster cutoff audio message going first, then 100ms later,
      // while it's playing, issue the command that starts
      // the right background noises, so they'll appear when the
      // former runs out.
      schedule_us(100000, (ActivationFuncPtr)ambient_noise_boost_complete,
                  &booster->ambient_noise);
    } else if (booster->bcontext == bcontext_docking) {
      ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                      sound_booster_flameout, sound_silence);
    } else {
      assert(false);
    }

    screenblanker_setmode(booster->screenblanker, sb_inactive);
  }
}
