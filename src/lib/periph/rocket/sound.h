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

#pragma once

#include "core/rulos.h"

#define SOUND(symbol, source_file_name, filter, label) symbol,

typedef enum e_sound_token {
#include "sound.def"

  sound_num_tokens
} SoundToken;

// The record format of the index at the beginning of the SD card.
// (The first record, of the same size, is a magic value; the next
// record is the index for SoundToken 0.)
typedef struct {
  uint32_t start_offset;
  uint32_t end_offset;
  uint16_t block_offset;
  uint16_t is_disco;
} AuIndexRec;

// The client code is written as if there are multiple channels (streams)
// that can be mixed together. The audio driver code doesn't yet support that,
// but these defines are sent in by the clients anyway.
// Okay, now the plan is to have, if not blended, then preemptive streams.
#define AUDIO_STREAM_BACKGROUND 0
#define AUDIO_STREAM_MUSIC 1
#define AUDIO_STREAM_BURST_EFFECTS 2
#define AUDIO_NUM_STREAMS 3

#define VOL_MAX (0)
#define VOL_MIN (7)
// VOL_MIN==7: it's the largest integer value, but the quietest sound
// (because volume is minus logarithm; output = value >> volume).
