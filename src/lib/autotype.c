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

#include "autotype.h"

void update_autotype(Autotype *a);

void init_autotype(Autotype *a, InputInjectorIfc *iii, char *str, Time period)
{
	a->func = (ActivationFunc) update_autotype;
	a->iii = iii;
	a->str = str;
	a->period = period;
	a->ptr = a->str;
	schedule_us(a->period, (Activation*) a);
}

void update_autotype(Autotype *a)
{
	char c = *(a->ptr);
	if (c!='\0')
	{
		a->ptr += 1;
		LOGF((logfp, "Autotype(%c)\n", c));
		a->iii->func(a->iii, c);
		schedule_us(a->period, (Activation*) a);
	}
}
