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

#ifndef _EVENT_H
#define _EVENT_H

#include <rocket.h>

typedef struct {
	r_bool auto_reset;
	r_bool signaled;
	ActivationFuncPtr waiter_func;
	void *waiter_data;
} Event;

void event_init(Event *evt, r_bool auto_reset);
void event_signal(Event *evt);
void event_reset(Event *evt);
void event_wait(Event *evt, ActivationFuncPtr func, void *data);

static inline r_bool event_is_signaled(Event *evt)
{
	return evt->signaled;
}

#endif // _EVENT_H

