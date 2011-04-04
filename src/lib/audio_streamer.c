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

#include "audio_streamer.h"
#include "event.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define R_SYNCDEBUG()	syncdebug(0, 'R', __LINE__)
#define SYNCDEBUG()	{R_SYNCDEBUG();}
//#define SYNCDEBUG()	{}
extern void audioled_set(r_bool red, r_bool yellow);

void _as_fill(AudioStreamer *as);

void as_1_init_sdc(AudioStreamer *as);
void as_2_check_reset(AudioStreamer *as);
void as_3_loop(AudioStreamer *as);
void as_4_start_tx(AudioStreamer *as);
void as_5_more_frames(AudioStreamer *as);
void as_6_read_more(AudioStreamer *as);

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id)
{
	as->timer_id = timer_id;
	init_audio_out(&as->audio_out, as->timer_id, (ActivationFuncPtr) _as_fill, as);
	as->sdc_initialized = FALSE;
	sdc_init(&as->sdc);

	event_init(&as->ulawbuf_empty_evt, FALSE);
	event_init(&as->play_request_evt, TRUE);

	// allow the first read to proceed.
	event_signal(&as->ulawbuf_empty_evt);

	// start the ASStreamCard thread
	schedule_now((ActivationFuncPtr) as_1_init_sdc, as);
}

void _ad_decode_ulaw_buf(uint8_t *dst, uint8_t *src, uint16_t len, uint8_t mlvolume)
{
	uint8_t *srcp = src;
	uint8_t *end = src+len;

	end = src+len;
	for (; srcp<end; srcp++, dst++)
	{
		uint8_t v = (*srcp) + 128;
		*dst = v >> mlvolume;
	}
}

void _as_fill(AudioStreamer *as)
{
	uint8_t *fill_ptr = as->audio_out.buffers[as->audio_out.fill_buffer];
//	SYNCDEBUG();
//	syncdebug(0, 'f', (int) as->audio_out.fill_buffer);
//	syncdebug(0, 'f', (int) fill_ptr);

//	audioled_set(0, 1);
	// always clear buf in case we end up coming back around here
	// before ASStreamCard gets a chance to fill it. Want to play
	// silence, not stuttered audio.
	memset(fill_ptr, 127, AO_BUFLEN);

	if (!event_is_signaled(&as->ulawbuf_empty_evt))
	{
		// invariant: ulawbuf_ready == sdc_ready waiting for it;
		// not elsewise scheduled.
//		audioled_set(1, 0);
		SYNCDEBUG();
		_ad_decode_ulaw_buf(fill_ptr, as->ulawbuf, AO_BUFLEN,
			as->is_music ? as->music_mlvolume : 0);
		event_signal(&as->ulawbuf_empty_evt);
	}
}

//////////////////////////////////////////////////////////////////////////////

void as_1_init_sdc(AudioStreamer *as)
{
	SYNCDEBUG();
	sdc_reset_card(&as->sdc, (ActivationFuncPtr) as_2_check_reset, as);
	return;
}

void as_2_check_reset(AudioStreamer *as)
{
	SYNCDEBUG();
	if (as->sdc.error)
	{
		R_SYNCDEBUG();
		schedule_now((ActivationFuncPtr) as_1_init_sdc, as);
	}
	else
	{
		schedule_now((ActivationFuncPtr) as_3_loop, as);
	}
}

void as_3_loop(AudioStreamer *as)
{
	SYNCDEBUG();
	as->sdc_initialized = TRUE;
	if (as->block_address >= as->end_address)
	{
		// just finished an entire file. Tell caller, and wait
		// for a new request.
		if (as->done_func != NULL)
		{
			schedule_now(as->done_func, as->done_data);
		}

		SYNCDEBUG();
		event_wait(&as->play_request_evt, (ActivationFuncPtr) as_3_loop, as);
		//SEQNEXT(ASStreamCard, as_stream_card, 2_loop);	// implied
		return;
	}

	// ready to start a read. Get an empty buffer first.
	SYNCDEBUG();
	event_wait(&as->ulawbuf_empty_evt, (ActivationFuncPtr) as_4_start_tx, as);
	return;
}

void as_4_start_tx(AudioStreamer *as)
{
	SYNCDEBUG();
	r_bool rc = sdc_start_transaction(&as->sdc,
		as->block_address,
		as->ulawbuf,
		AO_BUFLEN,
		(ActivationFuncPtr) as_5_more_frames,
		as);

	if (!rc)
	{
		// dang, card was busy! the 'goto' didn't (sdc hasn't taken
		// responsibility to call the continuation we passed in).
		// try again in a millisecond.
		SYNCDEBUG();
		schedule_us(1000, (ActivationFuncPtr) as_4_start_tx, as);
	}
}

void as_5_more_frames(AudioStreamer *as)
{
	SYNCDEBUG();
	if (sdc_is_error(&as->sdc))
	{
		SYNCDEBUG();
		sdc_end_transaction(&as->sdc, (ActivationFuncPtr) as_1_init_sdc, as);
		return;
	}

	event_reset(&as->ulawbuf_empty_evt);
	as->sector_offset += AO_BUFLEN;
	if (as->sector_offset < SDBUFSIZE)
	{
		// Wait for more space to put data into.
		SYNCDEBUG();
		event_wait(&as->ulawbuf_empty_evt, (ActivationFuncPtr) as_6_read_more, as);
		return;
	}

	// done reading sector; loop back around to read more
	// (or a different sector, if redirected)
	as->block_address += SDBUFSIZE;
	as->sector_offset = 0;

	SYNCDEBUG();
	sdc_end_transaction(&as->sdc, (ActivationFuncPtr) as_3_loop, as);
	return;
}

void as_6_read_more(AudioStreamer *as)
{
	sdc_continue_transaction(&as->sdc,
		as->ulawbuf,
		AO_BUFLEN,
		(ActivationFuncPtr) as_5_more_frames,
		as);
	return;
}

//////////////////////////////////////////////////////////////////////////////

r_bool as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset, uint32_t end_address, r_bool is_music, ActivationFuncPtr done_func, void *done_data)
{
	SYNCDEBUG();
	as->block_address = block_address;
	as->sector_offset = 0;
		// TODO reimplement preroll skippage
		// Just assigning this wouldn't help; it would cause us to
		// skip the END of the first block.
		// Maybe the best strategy is to end early rather than
		// start late.
	as->end_address = end_address;
	as->is_music = is_music;
	as->done_func = done_func;
	as->done_data = done_data;
		// TODO we just lose the previous callback in this case.
		// hope that's okay.
	event_signal(&as->play_request_evt);
	return TRUE;
}

void as_set_music_volume(AudioStreamer *as, uint8_t music_mlvolume)
{
	as->music_mlvolume = music_mlvolume;
}

void as_stop_streaming(AudioStreamer *as)
{
	as->end_address = 0;
}

SDCard *as_borrow_sdc(AudioStreamer *as)
{
	return as->sdc_initialized ? &as->sdc : NULL;
}
