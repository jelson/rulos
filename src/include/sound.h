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
} SoundToken;

//void sound_start(SoundToken token, r_bool loop);

#endif // _sound_h
