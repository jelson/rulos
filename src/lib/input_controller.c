#include "rocket.h"
#include "input_controller.h"

void input_poller_update(InputPollerAct *ip);

void input_poller_init(InputPollerAct *ip, InputInjectorIfc *injector)
{
	ip->func = (ActivationFunc) input_poller_update;
	ip->injector = injector;
	schedule_us(1, (Activation*) ip);
}

void input_poller_update(InputPollerAct *ip)
{
	char k = hal_read_keybuf();
	if (k!=0)
	{
		ip->injector->func(ip->injector, k);
	}
	schedule_us(50000, (Activation*) ip);
}

//////////////////////////////////////////////////////////////////////////////

void input_focus_injector_deliver(InputFocusInjector *ifi, char k);

void input_focus_injector_init(
	InputFocusInjector *ifi, UIEventHandler *topHandler)
{
	ifi->func = (InputInjectorFunc) input_focus_injector_deliver;
	ifi->topHandler = topHandler;
	ifi->topHandlerActive = FALSE;
}

void input_focus_injector_deliver(InputFocusInjector *ifi, char k)
{
	UIEventDisposition uied = uied_blur;
	if (ifi->topHandlerActive)
	{
		uied = ifi->topHandler->func(ifi->topHandler, k);
	}
	else if (k==uie_select || k==uie_right || k==uie_left)
	{
		uied = ifi->topHandler->func(ifi->topHandler, uie_focus);
	}
	switch (uied)
	{
		case uied_blur:
			ifi->topHandlerActive = FALSE;
			break;
		case uied_accepted:
			ifi->topHandlerActive = TRUE;
			break;
		default:
			assert(FALSE);
	}
}
