#include <inttypes.h>

#include "display_controller.h"
#include "display_compass.h"
#include "util.h"
#include "random.h"

void dcompass_update(DCompassAct *act);

void dcompass_init(DCompassAct *act, uint8_t board)
{
	act->func = (ActivationFunc) dcompass_update;
	act->board = board;
	act->offset = 0;
	schedule(0, (Activation*) act);
}

static char *compass_display = "N.,.3.,.S.,.E.,.N.,.3.,.S.,.E.,.";

void dcompass_update(DCompassAct *act)
{
	int8_t shift = ((int8_t)(deadbeef_rand() % 3)) - 1;
	act->offset = (act->offset+shift) & 0x0f;
	program_string(act->board, compass_display + act->offset);
	schedule(300+(deadbeef_rand() & 0x0ff), (Activation*) act);
}

