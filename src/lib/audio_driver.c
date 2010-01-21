#include "audio_driver.h"

uint16_t audio_refill_func(AudioDriver *ad, uint8_t *sample_buf, uint16_t count);
void _ad_decode_ulaw(uint8_t *pcm8u_buf, uint8_t *ulaw_buf, uint16_t count);
uint8_t _ad_pcm16s_to_pcm8u(int16_t s);

typedef struct {
	uint32_t start;
	uint32_t length;
} AudioClip;

#include "audio_index.ch"

#if 0
void init_audio_driver(AudioDriver *ad)
{
	ad->func = (HalAudioRefillFunc) audio_refill_func;
	ad->cur_token = sound_silence;
	ad->cur_offset = 0;
	ad->loop_token = sound_silence;
	ad->locked = FALSE;

	init_spiflash(&ad->spif, (Activation*) &ad->spiComplete);

	hal_audio_init(125, (HalAudioRefillIfc*) ad);
}

uint16_t audio_refill_func(AudioDriver *ad, uint8_t *sample_buf, uint16_t count)
{
	// called in interrupt context. Runs atomically, but we may be
	// interrupting a user-level call.
	if (ad->locked)
	{
		return 0;
	}

	AudioClip *ac = &audio_clips[ad->cur_token];
	if (ac->length - ad->cur_offset == 0)
	{
		LOGF((logfp, "refill; idx %d at %d\n", ad->cur_token, ad->cur_offset));
		// refill
		ad->cur_token = ad->loop_token;
		ad->cur_offset = 0;
		// recompute
		ac = &audio_clips[ad->cur_token];
	}
	uint16_t fill = min(count, ac->length - ad->cur_offset);
#if SIM
	_ad_decode_ulaw(sample_buf, buf_base+ac->start+ad->cur_offset, fill);
#else
//#error TODO No_SPI_flash_read_code_yet
#endif
	LOGF((logfp, "requested %d; satisfied %d bytes from %d.%d (%d)\n",
		count, fill, ad->cur_token, ad->cur_offset, ac->length));
	ad->cur_offset += fill;
	return fill;
}
#endif

void init_audio_driver(AudioDriver *ad)
{
	init_ring_buffer(ad->playback_buffer, sizeof(ad->playback_buffer_storage));
}

void audio_update(AudioDriver *ad)
{
	schedule(AUDIO_UPDATE_INTERVAL, (Activation*) ad);

	// jonh not very happy about blocking interrupts for collecting this stuff.
	hal_start_atomic();
	uint8_t output_insert_avail = ring_insert_avail(ad->output_buffer);
	hal_end_atomic();
	int i;
	uint8_t most_desperate_stream_index = -1;
	for (i=0; i<NUM_STREAMS; i++)
	{
		hal_start_atomic();
		stream_remove_avail[i] = ring_remove_avail(ad->stream[i]);
		hal_end_atomic();

		if (most_desperate_stream_index==-1 ||
			(stream_remove_avail[i]
				< stream_remove_avail[most_desperate_stream_index]))
		{
			most_desperate_stream_index = i;
		}
	}
	
	// inserts and removes can be done non-atomically, because they
	// only touch vars that we only ever update. (They do assert on
	// the opposite value, but in a way that is protected by monotonicity.)
	while (pa>0)
	{
		uint8_t composite_value = 0;
		for (i=0; i<NUM_STREAMS; i++)
		{
			if (stream_remove_avail[i] > 0)
			{
				stream_remove_avail[i]-=1;
				composite_value += _ad_decode_ulaw(ring_remove(ad->stream[i]));
			}
		}
		ring_insert(ad->output_buffer, composite_value);
		pa -= 1;
	}

	if (spi_next_buf_ptr == SPI_NOTHING)
	{
		uint8_t desperate_stream_insert_avail =
			ring_insert_avail(ad->stream[most_desperate_stream_index]);

		SPIBufPtr *spib = the_idle_one;
		if (desperate_stream_insert_avail > SPI_READ_SIZE)
		{
			spib->ring = ad->stream[most_desperate_stream_index];
			spib->request_len = SPI_READ_SIZE;
			also_figure_out_source_address;
		}
		hal_start_atomic();
		spi_next_buf_ptr = spib;
		hal_end_atomic();
	}
}

// based on sample code at:
// http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html
int16_t _ad_ulaw2linear(uint8_t ulawbyte)
{
  static int32_t exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
  int8_t sign;
  uint8_t exponent;
  int32_t mantissa;
  int32_t sample;

  ulawbyte = ~ulawbyte;
  sign = (ulawbyte & 0x80);
  exponent = (ulawbyte >> 4) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0) { sample = -sample; }

  return sample;
}

uint8_t _ad_pcm16s_to_pcm8u(int16_t s)
{
	return (s>>8)+127;
}

uint8_t _ad_decode_ulaw(uint8_t ulaw)
{
	return _ad_pcm16s_to_pcm8u(_ad_ulaw2linear(ulaw_buf[i]));
}

#if 0
void _ad_decode_ulaw(uint8_t *pcm8u_buf, uint8_t *ulaw_buf, uint16_t count)
{
	uint16_t i;
	for (i=0; i<count; i++)
	{
		pcm8u_buf[i] = _ad_pcm16s_to_pcm8u(_ad_ulaw2linear(ulaw_buf[i]));
	}
}
#endif

void ad_skip_to_clip(
	AudioDriver *ad, SoundToken cur_token, SoundToken loop_token)
{
	assert(cur_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	assert(loop_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	ad->locked = TRUE;
	ad->cur_token = cur_token;
	ad->cur_offset = 0;
	ad->loop_token = loop_token;
	ad->locked = FALSE;
}

void ad_queue_loop_clip(AudioDriver *ad, SoundToken loop_token)
{
	assert(loop_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	ad->locked = TRUE;
	ad->loop_token = loop_token;
	ad->locked = FALSE;
}

