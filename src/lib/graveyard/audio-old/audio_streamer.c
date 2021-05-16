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

#include "periph/audio/audio_streamer.h"

#include "core/event.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define R_SYNCDEBUG() syncdebug(0, 'R', __LINE__)
#define SYNCDEBUG() \
  { R_SYNCDEBUG(); }
//#define SYNCDEBUG()	{}
extern void audioled_set(bool red, bool yellow);

static void as_fill(AudioStreamer *as);

void as_1_init_sdc(AudioStreamer *as);
void as_2_check_reset(AudioStreamer *as);
void as_3_loop(AudioStreamer *as);
void as_4_start_tx(AudioStreamer *as);
void as_5_more_frames(AudioStreamer *as);
void as_6_read_more(AudioStreamer *as);

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id) {
  as->timer_id = timer_id;
  init_audio_out(&as->audio_out, as->timer_id, (ActivationFuncPtr)as_fill, as);
  as->sdc_initialized = FALSE;
  sdc_init(&as->sdc);

  event_init(&as->ulawbuf_empty_evt, FALSE);
  event_init(&as->play_request_evt, TRUE);

  // allow the first read to proceed.
  event_signal(&as->ulawbuf_empty_evt);

  // start the ASStreamCard thread
  schedule_now((ActivationFuncPtr)as_1_init_sdc, as);
}

static void ad_decode_ulaw_buf(uint8_t *dst, uint8_t *src, uint16_t len,
                               uint8_t mlvolume) {
  uint8_t *srcp = src;
  uint8_t *end = src + len;
  uint8_t voloffset =
      (mlvolume == 8) ? 128 : (((1 << mlvolume) - 1) << (7 - mlvolume));

  end = src + len;
  for (; srcp < end; srcp++, dst++) {
    uint8_t v = (*srcp) + 128;
    *dst = (v >> mlvolume) + voloffset;
  }
}

#if BLEND
static void ad_blend_ulaw_buf(uint8_t *fill_ptr, uint8_t last_blend_value) {
  // lite_assert(AO_BUFLEN>8);	// wish I could do this statically.
  int i;
  for (i = 0; i < 8; i++) {
    fill_ptr[i] = (last_blend_value >> i) + (fill_ptr[i] >> (8 - i));
  }
}
#endif  // BLEND

static void as_fill(AudioStreamer *as) {
  uint8_t *fill_ptr = as->audio_out.buffers[as->audio_out.fill_buffer];
  //	SYNCDEBUG();
  //	syncdebug(0, 'f', (int) as->audio_out.fill_buffer);
  //	syncdebug(0, 'f', (int) fill_ptr);

  audioled_set(0, 1);
  // always clear buf in case we end up coming back around here
  // before ASStreamCard gets a chance to fill it. Want to play
  // silence, not stuttered audio.
  memset(fill_ptr, 127, AO_BUFLEN);

  if (!event_is_signaled(&as->ulawbuf_empty_evt)) {
    // invariant: ulawbuf_ready == sdc_ready waiting for it;
    // not elsewise scheduled.
    audioled_set(1, 0);
    SYNCDEBUG();
    ad_decode_ulaw_buf(fill_ptr, as->ulawbuf, AO_BUFLEN, as->mlvolume);
#if BLEND
    as->last_blend_value = fill_ptr[AO_BUFLEN - 1];
    if (as->blend) {
      ad_blend_ulaw_buf(fill_ptr, as->last_blend_value);
      as->blend = false;
    }
#endif  // BLEND
    event_signal(&as->ulawbuf_empty_evt);
  }
}

//////////////////////////////////////////////////////////////////////////////

void as_1_init_sdc(AudioStreamer *as) {
  SYNCDEBUG();
  sdc_reset_card(&as->sdc, (ActivationFuncPtr)as_2_check_reset, as);
  return;
}

void as_2_check_reset(AudioStreamer *as) {
  SYNCDEBUG();
  if (as->sdc.error) {
    R_SYNCDEBUG();
    schedule_now((ActivationFuncPtr)as_1_init_sdc, as);
  } else {
    schedule_now((ActivationFuncPtr)as_3_loop, as);
  }
}

void as_3_loop(AudioStreamer *as) {
  SYNCDEBUG();
  as->sdc_initialized = TRUE;
  if (as->block_address >= as->end_address) {
    // just finished an entire file. Tell caller, and wait
    // for a new request.
    if (as->done_func != NULL) {
      schedule_now(as->done_func, as->done_data);
    }

    SYNCDEBUG();
    event_wait(&as->play_request_evt, (ActivationFuncPtr)as_3_loop, as);
    // SEQNEXT(ASStreamCard, as_stream_card, 2_loop);	// implied
    return;
  }

  // ready to start a read. Get an empty buffer first.
  SYNCDEBUG();
  event_wait(&as->ulawbuf_empty_evt, (ActivationFuncPtr)as_4_start_tx, as);
  return;
}

void as_4_start_tx(AudioStreamer *as) {
  SYNCDEBUG();
  bool rc =
      sdc_start_transaction(&as->sdc, as->block_address, as->ulawbuf, AO_BUFLEN,
                            (ActivationFuncPtr)as_5_more_frames, as);

  if (!rc) {
    // dang, card was busy! the 'goto' didn't (sdc hasn't taken
    // responsibility to call the continuation we passed in).
    // try again in a millisecond.
    SYNCDEBUG();
    schedule_us(1000, (ActivationFuncPtr)as_4_start_tx, as);
  }
}

void as_5_more_frames(AudioStreamer *as) {
  SYNCDEBUG();
  if (sdc_is_error(&as->sdc)) {
    SYNCDEBUG();
    sdc_end_transaction(&as->sdc, (ActivationFuncPtr)as_1_init_sdc, as);
    return;
  }

  event_reset(&as->ulawbuf_empty_evt);
  as->sector_offset += AO_BUFLEN;
  if (as->sector_offset < SDBUFSIZE) {
    // Wait for more space to put data into.
    SYNCDEBUG();
    event_wait(&as->ulawbuf_empty_evt, (ActivationFuncPtr)as_6_read_more, as);
    return;
  }

  // done reading sector; loop back around to read more
  // (or a different sector, if redirected)
  as->block_address += SDBUFSIZE;
  as->sector_offset = 0;

  SYNCDEBUG();
  sdc_end_transaction(&as->sdc, (ActivationFuncPtr)as_3_loop, as);
  return;
}

void as_6_read_more(AudioStreamer *as) {
  sdc_continue_transaction(&as->sdc, as->ulawbuf, AO_BUFLEN,
                           (ActivationFuncPtr)as_5_more_frames, as);
  return;
}

//////////////////////////////////////////////////////////////////////////////

bool as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset,
             uint32_t end_address, ActivationFuncPtr done_func,
             void *done_data) {
  SYNCDEBUG();
  as->block_address = block_address;
  as->sector_offset = 0;
  // TODO reimplement preroll skippage
  // Just assigning this wouldn't help; it would cause us to
  // skip the END of the first block.
  // Maybe the best strategy is to end early rather than
  // start late.
  as->end_address = end_address;
  as->done_func = done_func;
  as->done_data = done_data;
  // TODO we just lose the previous callback in this case.
  // hope that's okay.

#if BLEND
  as->blend = true;
#endif  // BLEND

  event_signal(&as->play_request_evt);
  return TRUE;
}

void as_set_volume(AudioStreamer *as, uint8_t mlvolume) {
  as->mlvolume = mlvolume;
}

void as_stop_streaming(AudioStreamer *as) { as->end_address = 0; }

SDCard *as_borrow_sdc(AudioStreamer *as) {
  return as->sdc_initialized ? &as->sdc : NULL;
}
