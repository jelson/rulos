#ifndef _SLOW_BOOT_H
#define _SLOW_BOOT_H

#include "screenblanker.h"
#include "audio_server.h"

#define SLOW_MAX_BUFFERS 7

typedef struct s_slow_boot {
	ActivationFunc func;
	ScreenBlanker *screenblanker;
	BoardBuffer buffer[SLOW_MAX_BUFFERS];
	Time startTime;
	AudioClient *audioClient;
} SlowBoot;

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker, AudioClient *audioClient);

#endif // _SLOW_BOOT_H
