#ifndef _AUDIO_STREAMER_H
#define _AUDIO_STREAMER_H

#include "rocket.h"
#include "sdcard.h"
#include "audio_out.h"

struct s_AudioStreamer;

typedef struct
{
	Activation act;
	struct s_AudioStreamer *as;
} InitializeCompleteAct;

typedef struct
{
	Activation act;
	struct s_AudioStreamer *as;
} StartStreamingAct;
	// when first sdc read completes, we can signal _fill to begin
	// streaming data to DAC.

// ugh; TODO clean up sdcard to hide fetch of stupid crc bytes

typedef struct s_AudioStreamer
{
	Activation fill_act;
	InitializeCompleteAct initialize_complete;
	StartStreamingAct start_streaming;
	SDCard sdc;
	AudioOut audio_out;
	r_bool streaming;
	uint8_t timer_id;
	uint32_t block_address;
		// 0 == playing silence
	uint32_t end_address;
	uint16_t block_offset;
	Activation *done_act;
} AudioStreamer;

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id);

// NB we can't really afford two 512-byte SDCard buffers, so
// we need to start the next SDCard transfer early enough that it
// arrives before its bytes are needed.
// Thus we set AO_HALFBUFLEN to provide enough preroll.
// 128 samples => 16ms.
// If that's not enough, 256*2 = 512 ugh, almost might as well
// double-buffer SDCard.
// An alternative: use 4 or 8 smaller AudioOut buffers, so that we can let
// them almost drain out during the time we're waiting on the sd card.

r_bool as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset, uint32_t end_address, Activation *done_act);
	// block_address: multiple of SDCard blocksize (512)
	// block_offset: multiple of AudioOut AO_HALFBUFLEN (128). Used if we
	//   want to have sound durations of shorter than SD block length.
	//   (Still a 16ms granularity.) Note that the padding goes at the
	//   beginning of the audio.
	// end_address: multiple of SDCard blocksize, the address of the
	//   byte *after* the last byte of the last block of this audio file.
	//  done_act: called once we've queued the last buffer for this sound.

void as_stop_streaming(AudioStreamer *as);

#endif // _AUDIO_STREAMER_H
