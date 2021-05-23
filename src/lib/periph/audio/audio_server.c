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

#include "periph/audio/audio_server.h"

#include <stdbool.h>

void aserv_recv_arm(MessageRecvBuffer *msg);
void aserv_recv_avm(MessageRecvBuffer *msg);
void aserv_recv_mcm(MessageRecvBuffer *msg);

void aserv_fetch_start(AudioServer *as);
void aserv_fetch_complete(AudioServer *as);
void aserv_start_play(AudioServer *as);
void aserv_advance(AudioServer *as);

static int count_music(AudioServer *aserv, int limit, char *opt_path,
                       int opt_path_capacity);

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

  // mount the FAT filesystem from the SD card.
  // Must be mounted before using audio_streamer.
  if (f_mount(&aserv->fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }

  // Initialize the streamer that actually plays the samples we schedule from
  // here.
  init_audio_streamer(&aserv->audio_streamer);

  // Initialize the streams to all play silence.
  for (uint8_t stream_idx = 0; stream_idx < AUDIO_NUM_STREAMS; stream_idx++) {
    aserv->audio_stream[stream_idx].skip_effect_id = sound_silence;
    aserv->audio_stream[stream_idx].loop_effect_id = sound_silence;
    aserv->audio_stream[stream_idx].volume = 0;
  }

  // Make background silent until instructed otherwise.
  aserv->audio_stream[AUDIO_STREAM_BACKGROUND].volume = 8;

  // Remember to seed the RNG when music is requested later.
  aserv->music_random_seeded = FALSE;

  aserv->music_file_count = count_music(aserv, 1000, NULL, 0);
}

// Concatenate onto dst, without overflowing capacity, and
// enforcing that dst stays nul-terminated.
void safecat(char *dst, int capacity, char *src) {
  strncat(dst, src, capacity - strlen(src));
  dst[capacity - 1] = '\0';
}

// Count all the music (with a huge limit), or count up to a particular
// pathname.
// if rc < limit && opt_path!=null, opt_path contains the limit'th path
// in the directory.
static int count_music(AudioServer *aserv, int limit, char *opt_path,
                       int opt_path_capacity) {
  DIR dp;
  FILINFO fno;
  int count = 0;

#define MUSIC_DIR "/music"
  FRESULT rc = f_opendir(&dp, MUSIC_DIR);
  assert(rc == FR_OK);

  while (true) {
    rc = f_readdir(&dp, &fno);
    if (rc != FR_OK || fno.fname[0] == '\0') {
      break;
    }
    if (count == limit) {
      // Hey, this is the one they wanted!
      if (opt_path != NULL) {
        opt_path[0] = '\0';
        safecat(opt_path, opt_path_capacity, MUSIC_DIR);
        safecat(opt_path, opt_path_capacity, "/");
        safecat(opt_path, opt_path_capacity, fno.fname);
      }
      break;
    }
    count += 1;
  }

  f_closedir(&dp);
  return count;
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

  LOG("audio_server receives index %d arm skip %d loop %d volume %d",
    arm->stream_idx, arm->skip_effect_id, arm->loop_effect_id,
    arm->volume);
  aserv->audio_stream[arm->stream_idx].volume = arm->volume;
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
  aserv->audio_stream[avm->stream_idx].volume = avm->volume;
  if (aserv->active_stream == avm->stream_idx) {
    LOG("aserv_recv_avm setting volume idx %d vol %d",
      avm->stream_idx, avm->volume);
    as_set_volume(&aserv->audio_streamer, avm->volume);
  }

  net_free_received_message_buffer(msg);
}

void aserv_recv_mcm(MessageRecvBuffer *msg) {
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(MusicControlMessage));
  MusicControlMessage *mcm = (MusicControlMessage *)&msg->data;

  if (!aserv->music_random_seeded) {
    // first use? jump to a random song. random seed is 1/10ths of sec since
    // boot.
    aserv->music_offset = (SoundEffectId)(
        ((clock_time_us() / 100000) % (aserv->music_file_count)));
    aserv->music_random_seeded = TRUE;
  }

  // record the embedded volume setting
  aserv->audio_stream[AUDIO_STREAM_MUSIC].volume = mcm->volume;

  // now advance one or retreat one, based on request
  aserv->music_offset =
      (aserv->music_offset + mcm->advance + aserv->music_file_count) %
      (aserv->music_file_count);
  aserv->audio_stream[AUDIO_STREAM_MUSIC].skip_effect_id = sound_music;

  // and start it playin'
  aserv_skip_stream(aserv, AUDIO_STREAM_MUSIC);

  net_free_received_message_buffer(msg);
}

static void find_music_filename(AudioServer *aserv, char *out_path,
                                int out_capacity) {
  if (aserv->active_stream == AUDIO_STREAM_MUSIC) {
    // filename is the n'th file in the directory
    int rc = count_music(aserv, aserv->music_offset, out_path, out_capacity);
    assert(rc == aserv->music_offset);  // someone lost count!
    // LOG("music %d / %d is %s", aserv->music_offset, aserv->music_file_count,
    // out_path);
  } else {
    // filename derives from a token index in lib/periph/audio/sound.def
    char asciiId[10];
    AudioEffectsStream *stream = &aserv->audio_stream[aserv->active_stream];
    itoda(asciiId, stream->skip_effect_id);

    out_path[0] = '\0';
    safecat(out_path, out_capacity, "sfx/");
    safecat(out_path, out_capacity, asciiId);
    safecat(out_path, out_capacity, ".raw");
  }
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
#define MAX_PATH 30
    char effect_filename[MAX_PATH];
    find_music_filename(aserv, effect_filename, MAX_PATH);

    as_set_volume(&aserv->audio_streamer, stream->volume);
    LOG("audio_server stream %d volume %d", aserv->active_stream, stream->volume);
    bool rc = as_play(&aserv->audio_streamer, effect_filename,
                      (ActivationFuncPtr)aserv_advance, aserv);
    if (!rc) {
      // Retry rapidly, so we can get ahold of sdc as soon as it's
      // idle. (Yeah, I could have a callback from SD to alert the
      // next waiter, but what a big project. This'll do.)
      schedule_us(10000, (ActivationFuncPtr)aserv_start_play, aserv);
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
