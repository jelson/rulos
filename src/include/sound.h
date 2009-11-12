#ifndef _sound_h
#define _sound_h

#include "rocket.h"

typedef enum e_sound_token {
// START_SOUND_TOKEN_ENUM
	sound_silence = 0,
	sound_pong_score,
	sound_pong_paddle_bounce,
	sound_pong_wall_bounce,
	sound_booster_start,
	sound_booster_running,
	sound_booster_flameout,
	sound_dock_clang,
// END_SOUND_TOKEN_ENUM
} SoundToken;

//void sound_start(SoundToken token, r_bool loop);

#endif // _sound_h
