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

#include <stdbool.h>
#include "event.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
//#define SYNCDEBUG()	syncdebug(0, 'E', __LINE__)
#define SYNCDEBUG()	{}

void event_init(Event *evt, r_bool auto_reset)
{
	evt->auto_reset = auto_reset;
	evt->signaled = false;
	evt->waiter = NULL;
}

void event_signal(Event *evt)
{
	if (evt->waiter != NULL)
	{
		SYNCDEBUG();
		schedule_now(evt->waiter);
		evt->waiter = NULL;
	}
	else
	{
		SYNCDEBUG();
		evt->signaled = true;
	}
}

void event_reset(Event *evt)
{
	evt->signaled = false;
}

void event_wait(Event *evt, Activation *act)
{
	if (evt->signaled)
	{
		SYNCDEBUG();
		schedule_now(act);
		if (evt->auto_reset)
		{
			evt->signaled = false;
		}
	}
	else
	{
		SYNCDEBUG();
		evt->waiter = act;
	}
}

