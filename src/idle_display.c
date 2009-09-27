#include <stdio.h>
#include <string.h>
#include "util.h"
#include "idle_display.h"
#include "display_scroll_msg.h"

void idle_display_update(IdleDisplayAct *act);
void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct, CpumonAct *cpumonAct)
{
	act->func = (ActivationFunc) idle_display_update;
	act->scrollAct = scrollAct;
	act->cpumonAct = cpumonAct;
	schedule(1, (Activation*) act);
}

void idle_display_update(IdleDisplayAct *act)
{
	strcpy(act->msg, "busy ");
	int d = int_to_string(act->msg+5, 2, FALSE, 100-cpumon_get_idle_percentage(act->cpumonAct));
	strcpy(act->msg+5+d, "%");
	dscrlmsg_set_msg(act->scrollAct, act->msg);
	schedule(1000, (Activation*) act);
}

