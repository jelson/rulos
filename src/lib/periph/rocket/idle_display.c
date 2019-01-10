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

#include "periph/rocket/idle_display.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/rocket.h"

void idle_display_update(IdleDisplayAct *act);
void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct,
                       CpumonAct *cpumonAct) {
  act->scrollAct = scrollAct;
  act->cpumonAct = cpumonAct;
  schedule_us(1, (ActivationFuncPtr)idle_display_update, act);
}

void idle_display_update(IdleDisplayAct *act) {
  strcpy(act->msg, "busy ");
  int d = int_to_string2(act->msg + 5, 2, 0,
                         100 - cpumon_get_idle_percentage(act->cpumonAct));
  strcpy(act->msg + 5 + d, "%");
  dscrlmsg_set_msg(act->scrollAct, act->msg);
  schedule_us(1000000, (ActivationFuncPtr)idle_display_update, act);
}
