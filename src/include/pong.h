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

#ifndef _pong_h
#define _pong_h

#include "rocket.h"
#include "audio_server.h"
#include "screen4.h"

#define PONG_HEIGHT 4
#define PONG_SCALE2 6
#define PONG_SCALE (1<<PONG_SCALE2)
#define PONG_FREQ2	5
#define PONG_FREQ (1<<PONG_FREQ2)

typedef struct s_pong_handler {
	UIEventHandlerFunc func;
	struct s_pong *pong;
} PongHandler;

typedef struct s_pong {
	ActivationFunc func;
//	BoardBuffer bbuf[PONG_HEIGHT];
//	BoardBuffer *btable[PONG_HEIGHT];
//	RectRegion rrect;
	Screen4 *s4;
	PongHandler handler;
	int x, y, dx, dy;	// scaled by PONG_SCALE
	int paddley[2];
	int score[2];
	Time lastScore;
	r_bool focused;
	AudioClient *audioClient;
} Pong;

void pong_init(Pong *pong, Screen4 *s4, AudioClient *audioClient);

#endif // _pong_h
