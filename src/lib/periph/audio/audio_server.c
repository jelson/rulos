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

#include <stdbool.h>

#include "periph/audio/audio_server.h"

void aserv_recv_arm(MessageRecvBuffer *msg);
void aserv_recv_avm(MessageRecvBuffer *msg);
void aserv_recv_mcm(MessageRecvBuffer *msg);

void aserv_fetch_start(AudioServer *as);
void aserv_fetch_complete(AudioServer *as);
void aserv_start_play(AudioServer *as);
void aserv_advance(AudioServer *as);

void init_audio_server(AudioServer *aserv, Network *network, uint8_t timer_id) {
  // Listen on network socket for AudioRequestMessage to play sound effects.
  aserv->arm_app_receiver.recv_complete_func = aserv_recv_arm;
  aserv->arm_app_receiver.port = AUDIO_PORT;
  aserv->arm_app_receiver.num_receive_buffers = 1;
  aserv->arm_app_receiver.payload_capacity = sizeof(AudioRequestMessage);
  aserv->arm_app_receiver.message_recv_buffers = aserv->arm_recv_ring_alloc;
  aserv->arm_app_receiver.user_data = aserv;
  net_bind_receiver(network, &aserv->arm_app_receiver);

  // Listen on network socket for AudioVolumeMessage to adjust music volume.
  aserv->avm_app_receiver.recv_complete_func = aserv_recv_avm;
  aserv->avm_app_receiver.port = SET_VOLUME_PORT;
  aserv->avm_app_receiver.num_receive_buffers = 1;
  aserv->avm_app_receiver.payload_capacity = sizeof(AudioVolumeMessage);
  aserv->avm_app_receiver.message_recv_buffers = aserv->avm_recv_ring_alloc;
  aserv->avm_app_receiver.user_data = aserv;
  net_bind_receiver(network, &aserv->avm_app_receiver);

  // Listen on network socket for MusicControlMessage to change disco music
  // track.
  aserv->mcm_app_receiver.recv_complete_func = aserv_recv_mcm;
  aserv->mcm_app_receiver.port = MUSIC_CONTROL_PORT;
  aserv->mcm_app_receiver.num_receive_buffers = 1;
  aserv->mcm_app_receiver.payload_capacity = sizeof(MusicControlMessage);
  aserv->mcm_app_receiver.message_recv_buffers = aserv->mcm_recv_ring_alloc;
  aserv->mcm_app_receiver.user_data = aserv;
  net_bind_receiver(network, &aserv->mcm_app_receiver);

  // Initialize the streamer that actually plays the samples we schedule from
  // here.
  init_audio_streamer(&aserv->audio_streamer);

  // Initialize the streams to all play silence.
  for (uint8_t stream_idx = 0; stream_idx < AUDIO_NUM_STREAMS; stream_idx++) {
    aserv->audio_stream[stream_idx].skip_effect_id = sound_silence;
    aserv->audio_stream[stream_idx].loop_effect_id = sound_silence;
    aserv->audio_stream[stream_idx].mlvolume = 0;
  }

  // Make background silent until instructed otherwise.
  aserv->audio_stream[AUDIO_STREAM_BACKGROUND].mlvolume = 8;

  // Remember to seed the RNG when music is requested later.
  aserv->music_random_seeded = FALSE;
}

void aserv_skip_stream(AudioServer *aserv, uint8_t stream_idx) {
  if (aserv->audio_stream[stream_idx].skip_effect_id != sound_silence &&
      stream_idx >= aserv->active_stream) {
    // preemption
    aserv->active_stream = stream_idx;
    aserv_start_play(aserv);
  } else if (aserv->audio_stream[stream_idx].skip_effect_id == sound_silence &&
             stream_idx == aserv->active_stream) {
    // drop back down the stack and find something else to pick up
    aserv_advance(aserv);
  }
}

void aserv_recv_arm(MessageRecvBuffer *msg) {
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(AudioRequestMessage));
  AudioRequestMessage *arm = (AudioRequestMessage *)&msg->data;
  assert(arm->stream_idx < AUDIO_NUM_STREAMS);

  aserv->audio_stream[arm->stream_idx].loop_effect_id = arm->loop_effect_id;
  if (arm->skip) {
    aserv->audio_stream[arm->stream_idx].skip_effect_id = arm->skip_effect_id;
    aserv_skip_stream(aserv, arm->stream_idx);
  }

  net_free_received_message_buffer(msg);
}

void aserv_recv_avm(MessageRecvBuffer *msg) {
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(AudioVolumeMessage));
  AudioVolumeMessage *avm = (AudioVolumeMessage *)&msg->data;

  assert(avm->stream_idx < AUDIO_NUM_STREAMS);
  aserv->audio_stream[avm->stream_idx].mlvolume = avm->mlvolume;
  if (aserv->active_stream == avm->stream_idx) {
    as_set_volume(&aserv->audio_streamer, avm->mlvolume);
  }

  net_free_received_message_buffer(msg);
}

void aserv_recv_mcm(MessageRecvBuffer *msg) {
#if 0  // until music works again
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(MusicControlMessage));
  MusicControlMessage *mcm = (MusicControlMessage *)&msg->data;

    if (!aserv->music_random_seeded) {
      // first use? jump to a random song. random seed is 1/10ths of sec since
      // boot.
      aserv->music_offset = (SoundEffectId)(
          ((clock_time_us() / 100000) % (aserv->num_music_tokens)));
      aserv->music_random_seeded = TRUE;
    }

    // now advance one or retreat one, based on request
    aserv->music_offset =
        (aserv->music_offset + mcm->advance + aserv->num_music_tokens) %
        (aserv->num_music_tokens);
    aserv->audio_stream[AUDIO_STREAM_MUSIC].skip_effect_id =
        (SoundEffectId)(aserv->music_offset + aserv->music_first_token);

    // and start it playin'
    aserv_skip_stream(aserv, AUDIO_STREAM_MUSIC);
#endif

  net_free_received_message_buffer(msg);
}

// Time to stop whatever else we were playing (because it finished, or because
// an incoming request preempted it), see what should be played right now, and
// start playing it.
void aserv_start_play(AudioServer *aserv) {
  AudioEffectsStream *stream = &aserv->audio_stream[aserv->active_stream];
  if (stream->skip_effect_id == sound_silence) {
    as_stop_streaming(&aserv->audio_streamer);
  } else if (stream->skip_effect_id < 0 ||
             stream->skip_effect_id >= sound_num_ids) {
    // error: invalid token.
    stream->skip_effect_id = sound_silence;
    as_stop_streaming(&aserv->audio_streamer);
  } else {
    char effect_filename[25];
    strcpy(effect_filename, "sfx/");
    itoda(effect_filename + strlen(effect_filename), stream->skip_effect_id);
    strcat(effect_filename, ".raw");
    as_set_volume(&aserv->audio_streamer, stream->mlvolume);
    r_bool rc = as_play(&aserv->audio_streamer, effect_filename,
                        (ActivationFuncPtr)aserv_advance, aserv);
    if (!rc) {
      // Retry rapidly, so we can get ahold of sdc as soon as it's
      // idle. (Yeah, I could have a callback from SD to alert the
      // next waiter, but what a big project. This'll do.)
      schedule_us(1, (ActivationFuncPtr)aserv_start_play, aserv);
    }
  }
}

void aserv_advance(AudioServer *aserv) {
  aserv->audio_stream[aserv->active_stream].skip_effect_id =
      aserv->audio_stream[aserv->active_stream].loop_effect_id;
  // drop down the stream stack until we find some non-silence.
  for (; aserv->active_stream >= 0 &&
         aserv->audio_stream[aserv->active_stream].skip_effect_id ==
             sound_silence;
       aserv->active_stream--) {
  }

  if (aserv->active_stream < 0) {
    aserv->active_stream = 0;
  }

  aserv_start_play(aserv);
}
