#ifndef _SCREENBLANKER_H
#define _SCREENBLANKER_H

#include "rocket.h"
#include "idle.h"
#include "hpam.h"

#define MAX_BUFFERS 8

typedef struct {
	UIEventHandlerFunc func;
	BoardBuffer buffer[MAX_BUFFERS];
	uint8_t num_buffers;
} ScreenBlanker;

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle);

#endif // _SCREENBLANKER_H
