#ifndef _sound_h
#define _sound_h

#include "rocket.h"

typedef enum {
	sound_silence,
	sound_pong_score,
	sound_pong_paddle_bounce,
	sound_pong_wall_bounce,
	sound_launch_noise,
	sound_klaxon,
	sound_dock_clang,
} SoundToken;

void sound_start(SoundToken token, r_bool loop);

#endif // _sound_h
