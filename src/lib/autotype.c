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
