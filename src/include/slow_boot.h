#ifndef _SLOW_BOOT_H
#define _SLOW_BOOT_H

#include "screenblanker.h"
#include "audio_server.h"

#define SLOW_MAX_BUFFERS 7
#define BORROW_SCREENBLANKER_BUFS 1

typedef struct s_slow_boot {
	ActivationFunc func;
	ScreenBlanker *screenblanker;
#if BORROW_SCREENBLANKER_BUFS
	BoardBuffer *buffer;
#else // BORROW_SCREENBLANKER_BUFS
	BoardBuffer buffer[SLOW_MAX_BUFFERS];
#endif // BORROW_SCREENBLANKER_BUFS
	Time startTime;
	AudioClient *audioClient;
} SlowBoot;

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker, AudioClient *audioClient);

#endif // _SLOW_BOOT_H
