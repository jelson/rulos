#ifndef _MOMENTARY_DISPLAY_H
#define _MOMENTARY_DISPLAY_H

#include "rocket.h"
#include "network.h"
#include "momentary_display_message.h"

typedef struct {
	Time display_period;
	uint8_t board_num;

	uint8_t messagespace[sizeof(MomentaryDisplayMessage)];
	RecvSlot recvSlot;

	BoardBuffer bbuf;
	r_bool is_visible;
	Time last_display;
} MomentaryDisplay;

#endif // _MOMENTARY_DISPLAY_H
