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

#define FETCH_RETRY_TIME 1000000

extern void syncdebug(uint8_t spaces, char f, uint16_t line);

#if defined(SIM) || defined(RULOS_ARM)
#define SYNCPRINT(x, y, z) \
  {}
#else
#define SYNCPRINT(x, y, z) syncdebug(x, y, z)
#endif

#define SYNCDEBUG() SYNCPRINT(0, 'U', __LINE__)

void ac_send_complete(SendSlot *sendSlot);

void aserv_recv_arm(MessageRecvBuffer *msg);
void aserv_recv_avm(MessageRecvBuffer *msg);
void aserv_recv_mcm(MessageRecvBuffer *msg);
void aserv_fetch_start(AudioServer *as);
void aserv_fetch_complete(AudioServer *as);
void aserv_start_play(AudioServer *as);
void aserv_advance(AudioServer *as);

void init_audio_server(AudioServer *aserv, Network *network, uint8_t timer_id) {
  aserv->arm_app_receiver.recv_complete_func = aserv_recv_arm;
  aserv->arm_app_receiver.port = AUDIO_PORT;
  aserv->arm_app_receiver.num_receive_buffers = 1;
  aserv->arm_app_receiver.payload_capacity = sizeof(AudioRequestMessage);
  aserv->arm_app_receiver.message_recv_buffers = aserv->arm_recv_ring_alloc;
  aserv->arm_app_receiver.user_data = aserv;

  aserv->avm_app_receiver.recv_complete_func = aserv_recv_avm;
  aserv->avm_app_receiver.port = SET_VOLUME_PORT;
  aserv->avm_app_receiver.num_receive_buffers = 1;
  aserv->avm_app_receiver.payload_capacity = sizeof(AudioVolumeMessage);
  aserv->avm_app_receiver.message_recv_buffers = aserv->avm_recv_ring_alloc;
  aserv->avm_app_receiver.user_data = aserv;

  aserv->mcm_app_receiver.recv_complete_func = aserv_recv_mcm;
  aserv->mcm_app_receiver.port = MUSIC_CONTROL_PORT;
  aserv->mcm_app_receiver.num_receive_buffers = 1;
  aserv->mcm_app_receiver.payload_capacity = sizeof(MusicControlMessage);
  aserv->mcm_app_receiver.message_recv_buffers = aserv->mcm_recv_ring_alloc;
  aserv->mcm_app_receiver.user_data = aserv;

  uint8_t stream_id;
  for (stream_id = 0; stream_id < AUDIO_NUM_STREAMS; stream_id++) {
    aserv->audio_stream[stream_id].skip_cmd.token = sound_silence;
    aserv->audio_stream[stream_id].loop_cmd.token = sound_silence;
    aserv->audio_stream[stream_id].mlvolume = 0;
  }
  aserv->audio_stream[AUDIO_STREAM_BACKGROUND].mlvolume = 8;

  aserv->music_random_seeded = FALSE;
  // none of these should be accessed until index ready,
  // but might as well load sane-ish values
  aserv->num_music_tokens = 1;
  aserv->music_first_token = sound_silence;

  init_audio_streamer(&aserv->audio_streamer, timer_id);
  aserv->index_ready = FALSE;
  aserv_fetch_start(aserv);

  net_bind_receiver(network, &aserv->arm_app_receiver);
  net_bind_receiver(network, &aserv->avm_app_receiver);
  net_bind_receiver(network, &aserv->mcm_app_receiver);
}

void aserv_fetch_start(AudioServer *aserv) {
  SYNCDEBUG();

  // try to borrow sdc object from audio_streamer
  aserv->borrowed_sdc = as_borrow_sdc(&aserv->audio_streamer);
  if (aserv->borrowed_sdc == NULL) {
    // sdc not initialized yet
    SYNCDEBUG();
    schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr)aserv_fetch_start, aserv);
    return;
  }

  uint8_t *start_addr = (uint8_t *)&aserv->magic;
  uint16_t count = sizeof(aserv->magic) + sizeof(aserv->index);
  r_bool rc =
      sdc_start_transaction(aserv->borrowed_sdc, 0, start_addr, count,
                            (ActivationFuncPtr)aserv_fetch_complete, aserv);
#if !SIM
  SYNCPRINT(4, 'a', (uint16_t)(aserv));
  SYNCPRINT(4, 'b', (uint16_t)(aserv->borrowed_sdc));
  SYNCPRINT(4, 's', (uint16_t)(start_addr));
  SYNCPRINT(4, 'c', (uint16_t)(count));
#endif
  if (!rc) {
    SYNCDEBUG();
    schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr)aserv_fetch_start, aserv);
    return;
  }
}

void aserv_fetch_complete(AudioServer *aserv) {
  SYNCDEBUG();
#if !SIM
  SYNCPRINT(4, 'a', (uint16_t)(aserv));
  SYNCPRINT(4, 'b', (uint16_t)(&aserv->borrowed_sdc));
  SYNCPRINT(4, 'b', (uint16_t)(aserv->borrowed_sdc));
#endif

  bool success = !aserv->borrowed_sdc->error;
  if (success) {
    int i;
    for (i = 0; i < sound_num_tokens; i++) {
      SYNCPRINT(0, 'i', i);
      SYNCPRINT(2, 'o', aserv->index[i].start_offset);
      if (aserv->index[i].is_disco) {
        aserv->music_first_token = i;
        break;
      }
    }
    for (i = aserv->music_first_token; i < sound_num_tokens; i++) {
      SYNCPRINT(0, 'j', i);
      if (!aserv->index[i].is_disco) {
        aserv->num_music_tokens = i - aserv->music_first_token;
        break;
      }
    }
    if (i == sound_num_tokens) {
      aserv->num_music_tokens = i - aserv->music_first_token;
    }

    aserv->index_ready = TRUE;
  }

  sdc_end_transaction(aserv->borrowed_sdc, NULL, NULL);

  if (!success) {
    SYNCDEBUG();
    schedule_us(FETCH_RETRY_TIME, (ActivationFuncPtr)aserv_fetch_start, aserv);
  }
  SYNCDEBUG();
}

void aserv_skip_stream(AudioServer *aserv, uint8_t stream_id) {
  SYNCDEBUG();
  if (aserv->audio_stream[stream_id].skip_cmd.token != sound_silence &&
      stream_id >= aserv->active_stream) {
    SYNCDEBUG();
    // preemption
    aserv->active_stream = stream_id;
    aserv_start_play(aserv);
  } else if (aserv->audio_stream[stream_id].skip_cmd.token == sound_silence &&
             stream_id == aserv->active_stream) {
    SYNCDEBUG();
    // drop back down the stack and find something else to pick up
    aserv_advance(aserv);
  }
}

void aserv_recv_arm(MessageRecvBuffer *msg) {
  SYNCDEBUG();
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(AudioRequestMessage));
  AudioRequestMessage *arm = (AudioRequestMessage *)&msg->data;
  assert(arm->stream_id < AUDIO_NUM_STREAMS);

  aserv->audio_stream[arm->stream_id].loop_cmd = arm->loop_cmd;
  if (arm->skip) {
    SYNCDEBUG();
    aserv->audio_stream[arm->stream_id].skip_cmd = arm->skip_cmd;
    SYNCPRINT(2, 'i', arm->stream_id);
    SYNCPRINT(2, 'k', aserv->audio_stream[arm->stream_id].skip_cmd.token);
    SYNCPRINT(2, 'l', aserv->audio_stream[arm->stream_id].loop_cmd.token);
    aserv_skip_stream(aserv, arm->stream_id);
  }

  net_free_received_message_buffer(msg);
}

void aserv_recv_avm(MessageRecvBuffer *msg) {
  SYNCDEBUG();
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(AudioVolumeMessage));
  AudioVolumeMessage *avm = (AudioVolumeMessage *)&msg->data;

  assert(avm->stream_id < AUDIO_NUM_STREAMS);
  aserv->audio_stream[avm->stream_id].mlvolume = avm->mlvolume;
  if (aserv->active_stream == avm->stream_id) {
    as_set_volume(&aserv->audio_streamer, avm->mlvolume);
  }

  net_free_received_message_buffer(msg);
}

void aserv_recv_mcm(MessageRecvBuffer *msg) {
  SYNCDEBUG();
  AudioServer *aserv = (AudioServer *)msg->app_receiver->user_data;
  assert(msg->payload_len == sizeof(MusicControlMessage));
  MusicControlMessage *mcm = (MusicControlMessage *)&msg->data;

  if (aserv->index_ready) {
    if (!aserv->music_random_seeded) {
      SYNCPRINT(0, 's', 0x5eed);
      // first use? jump to a random song. random seed is 1/10ths of sec since
      // boot.
      aserv->music_offset = (SoundToken)(
          ((clock_time_us() / 100000) % (aserv->num_music_tokens)));
      aserv->music_random_seeded = TRUE;
    }

    // now advance one or retreat one, based on request
    SYNCPRINT(1, 'o', aserv->music_offset);
    aserv->music_offset =
        (aserv->music_offset + mcm->advance + aserv->num_music_tokens) %
        (aserv->num_music_tokens);
    SYNCPRINT(1, 'o', aserv->music_offset);
    aserv->audio_stream[AUDIO_STREAM_MUSIC].skip_cmd.token =
        (SoundToken)(aserv->music_offset + aserv->music_first_token);

    SYNCPRINT(1, 'd', mcm->advance);
    SYNCPRINT(1, 't', aserv->music_offset);
    SYNCPRINT(1, 'n', aserv->num_music_tokens);
    // and start it playin'
    aserv_skip_stream(aserv, AUDIO_STREAM_MUSIC);
  }

  net_free_received_message_buffer(msg);
}

void aserv_dbg_play(AudioServer *aserv, SoundToken skip, SoundToken loop) {
  aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].skip_cmd.token = skip;
  aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].loop_cmd.token = loop;
  aserv->audio_stream[AUDIO_STREAM_BURST_EFFECTS].mlvolume = 0;
  aserv_start_play(aserv);
}

void aserv_start_play(AudioServer *aserv) {
  SYNCDEBUG();
  AudioEffectsStream *stream = &aserv->audio_stream[aserv->active_stream];
  if (stream->skip_cmd.token == sound_silence) {
    SYNCDEBUG();
    as_stop_streaming(&aserv->audio_streamer);
  } else if (stream->skip_cmd.token < 0 ||
             stream->skip_cmd.token >= sound_num_tokens) {
    // error: invalid token.
    stream->skip_cmd.token = sound_silence;
    as_stop_streaming(&aserv->audio_streamer);
    SYNCPRINT(0, 'a', 0xfedc);
  } else if (!aserv->index_ready) {
    SYNCDEBUG();
  } else {
    SYNCDEBUG();
#if !SIM
//		SYNCPRINT(4, 'd', (uint16_t) (&aserv->audio_streamer.sdc));
#endif
    //		SYNCPRINT(4, 'o', aserv->audio_streamer.sdc.transaction_open);

    AuIndexRec *airec = &aserv->index[stream->skip_cmd.token];
    //		SYNCPRINT(4, 's', airec->start_offset>>16);
    //		SYNCPRINT(4, 's', airec->start_offset&0xffff);
    //		SYNCPRINT(4, 'e', airec->end_offset>>16);
    //		SYNCPRINT(4, 'e', airec->end_offset&0xffff);
    SYNCPRINT(5, 'i', aserv->active_stream);
    SYNCPRINT(5, 't', stream->skip_cmd.token);
    SYNCPRINT(5, 'v', stream->mlvolume);
    as_set_volume(&aserv->audio_streamer, stream->mlvolume);
    r_bool rc = as_play(&aserv->audio_streamer, airec->start_offset,
                        airec->block_offset, airec->end_offset,
                        (ActivationFuncPtr)aserv_advance, aserv);
    if (!rc) {
      SYNCDEBUG();
      // Retry rapidly, so we can get ahold of sdc as soon as it's
      // idle. (Yeah, I could have a callback from SD to alert the
      // next waiter, but what a big project. This'll do.)
      schedule_us(1, (ActivationFuncPtr)aserv_start_play, aserv);
    }
  }
}

void aserv_advance(AudioServer *aserv) {
  aserv->audio_stream[aserv->active_stream].skip_cmd =
      aserv->audio_stream[aserv->active_stream].loop_cmd;
  // drop down the stream stack until we find some non-silence.
  for (; aserv->active_stream >= 0 &&
         aserv->audio_stream[aserv->active_stream].skip_cmd.token ==
             sound_silence;
       aserv->active_stream--) {
  }

  if (aserv->active_stream < 0) {
    aserv->active_stream = 0;
  }

  aserv_start_play(aserv);
}
