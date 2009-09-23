#include "input_controller.h"

char scan_keyboard();	// defined in sim.c, and ... where else? TODO include appropriate header
void input_controller_update(InputControllerAct *act);

void input_controller_init(InputControllerAct *act, InputHandler *topHandler)
{
	act->func = (ActivationFunc) input_controller_update;
	act->topHandler = topHandler;
	schedule(0, (Activation*) act);
}

void input_controller_update(InputControllerAct *act)
{
	char k = scan_keyboard();
	if (k!=0)
	{
		act->topHandler->func(act->topHandler, k);
	}
	
	schedule(100, (Activation*) act);
}
