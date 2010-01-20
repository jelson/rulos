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

typedef enum {
	DISCO_GREEN=1,
	DISCO_RED=2,
	DISCO_YELLOW=3,
	DISCO_BLUE=4,
	DISCO_WHITE=5,
} DiscoColor;

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
	uint32_t *tree;
	uint8_t disco_color;
} ScreenBlanker;

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle);
void screenblanker_setmode(ScreenBlanker *screenblanker, ScreenBlankerMode newmode);
void screenblanker_setdisco(ScreenBlanker *screenblanker, DiscoColor disco_color);

#endif // _SCREENBLANKER_H
