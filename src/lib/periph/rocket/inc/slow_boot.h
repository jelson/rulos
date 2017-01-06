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

#ifndef _SLOW_BOOT_H
#define _SLOW_BOOT_H

#include "screenblanker.h"
#include "audio_client.h"

#define SLOW_MAX_BUFFERS 7

#define BORROW_SCREENBLANKER_BUFS 1

typedef struct s_slow_boot {
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
