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

#ifndef _idle_display_h
#define _idle_display_h

#include "core/cpumon.h"
#include "periph/rocket/display_scroll_msg.h"

typedef struct {
	DScrollMsgAct *scrollAct;
	CpumonAct *cpumonAct;
	char msg[16];
} IdleDisplayAct;

void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct, CpumonAct *cpumonAct);

#endif // _idle_display_h
