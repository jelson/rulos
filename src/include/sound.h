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

#ifndef _sound_h
#define _sound_h

#include "rocket.h"

typedef enum e_sound_token {
// START_SOUND_TOKEN_ENUM
	sound_silence = 0,
	sound_pong_score,
	sound_pong_paddle_bounce,
	sound_pong_wall_bounce,
	sound_apollo_11_countdown,
	sound_booster_start,
	sound_booster_running,
	sound_booster_flameout,
	sound_dock_clang,
	sound_quindar_key_down,
	sound_quindar_key_up,
// END_SOUND_TOKEN_ENUM
	sound_num_tokens
} SoundToken;

// The record format of the index at the beginning of the SD card.
// (The first record, of the same size, is a magic value; the next
// record is the index for SoundToken 0.)
typedef struct {
	uint32_t start_offset;
	uint32_t end_offset;
	uint16_t block_offset;
	uint16_t padding;
} AuIndexRec;

// The client code is written as if there are multiple channels (streams)
// that can be mixed together. The audio driver code doesn't yet support that,
// but these defines are sent in by the clients anyway.
#define AUDIO_STREAM_BURST_EFFECTS		0
#define AUDIO_STREAM_CONTINUOUS_EFFECTS	1
#define AUDIO_STREAM_COUNTDOWN			2

#endif // _sound_h
