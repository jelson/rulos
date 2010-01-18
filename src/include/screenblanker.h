#ifndef _SCREENBLANKER_H
#define _SCREENBLANKER_H

#include "rocket.h"
#include "idle.h"
#include "hpam.h"

#define MAX_BUFFERS 8

struct s_screen_blanker;

typedef enum
{
	sb_inactive,	// not operating.
	sb_blankdots,	// all digits showing only SB_DECIMAL
	sb_black,		// all LED segments off; lights unaffected
	sb_disco,		// flicker panels & lights by color
	sb_flicker		// mostly on, with segments vanishing intermittently
} ScreenBlankerMode;

typedef struct {
	ActivationFunc func;
	struct s_screen_blanker *sb;
} ScreenBlankerClockAct;

typedef struct s_screen_blanker {
	UIEventHandlerFunc func;
	BoardBuffer buffer[MAX_BUFFERS];
	uint8_t hpam_max_alpha[MAX_BUFFERS];
	uint8_t num_buffers;
	ScreenBlankerClockAct clock_act;
	ScreenBlankerMode mode;
} ScreenBlanker;

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle);
void screenblanker_setmode(ScreenBlanker *screenblanker, ScreenBlankerMode newmode);

#endif // _SCREENBLANKER_H
