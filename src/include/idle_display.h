#ifndef _idle_display_h
#define _idle_display_h

#include "display_scroll_msg.h"
#include "cpumon.h"

typedef struct {
	ActivationFunc func;
	DScrollMsgAct *scrollAct;
	CpumonAct *cpumonAct;
	char msg[16];
} IdleDisplayAct;

void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct, CpumonAct *cpumonAct);

#endif // _idle_display_h
