#include "rocket.h"
#include "input_controller.h"

void input_controller_update(InputControllerAct *act);

void input_controller_init(InputControllerAct *act, UIEventHandler *topHandler)
{
	act->func = (ActivationFunc) input_controller_update;
	act->topHandler = topHandler;
	act->topHandlerActive = FALSE;
	schedule_us(1, (Activation*) act);
}

void input_controller_update(InputControllerAct *act)
{
	char k = hal_read_keybuf();
	if (k!=0)
	{
		UIEventDisposition uied = uied_blur;
		if (act->topHandlerActive)
		{
			uied = act->topHandler->func(act->topHandler, k);
		}
		else if (k==uie_select || k==uie_right || k==uie_left)
		{
			uied = act->topHandler->func(act->topHandler, uie_focus);
		}
		switch (uied)
		{
			case uied_blur:
				act->topHandlerActive = FALSE;
				break;
			case uied_accepted:
				act->topHandlerActive = TRUE;
				break;
			default:
				assert(FALSE);
		}
	}
	
	schedule_us(50000, (Activation*) act);
}