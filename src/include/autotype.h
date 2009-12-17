#ifndef _AUTOTYPE_H
#define _AUTOTYPE_H

#include "rocket.h"
#include "input_controller.h"

typedef struct s_autotype {
	ActivationFunc func;
	InputInjectorIfc *iii;
	Time period;
	char *str;
	char *ptr;
} Autotype;

void init_autotype(Autotype *a, InputInjectorIfc *iii, char *str, Time period);

#endif // _AUTOTYPE_H
