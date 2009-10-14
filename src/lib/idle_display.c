#include "rocket.h"
#include "idle_display.h"
#include "display_scroll_msg.h"

void idle_display_update(IdleDisplayAct *act);
void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct, CpumonAct *cpumonAct)
{
	act->func = (ActivationFunc) idle_display_update;
	act->scrollAct = scrollAct;
	act->cpumonAct = cpumonAct;
	schedule_us(1, (Activation*) act);
}

void idle_display_update(IdleDisplayAct *act)
{
	strcpy(act->msg, "busy ");
	int d = int_to_string2(act->msg+5, 2, 0, 100-cpumon_get_idle_percentage(act->cpumonAct));
	strcpy(act->msg+5+d, "%");
	dscrlmsg_set_msg(act->scrollAct, act->msg);
	schedule_us(1000000, (Activation*) act);
}

