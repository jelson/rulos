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

#ifndef _audio_request_message_h
#define _audio_request_message_h

#include "rocket.h"
#include "sound.h"

typedef struct {
	uint8_t stream_idx;
	r_bool skip;
	SoundToken skip_token;
	SoundToken loop_token;
} AudioRequestMessage;

#endif // _audio_request_message_h
