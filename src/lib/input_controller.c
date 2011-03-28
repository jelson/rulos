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

#include "rocket.h"
#include "input_controller.h"

void input_poller_update(InputPollerAct *ip);

void input_poller_init(InputPollerAct *ip, InputInjectorIfc *injector)
{
	hal_init_keypad();
	ip->injector = injector;
	schedule_us(1, (ActivationFuncPtr) input_poller_update, ip);
}

void input_poller_update(InputPollerAct *ip)
{
	char k = hal_read_keybuf();
	if (k!=0)
	{
		ip->injector->func(ip->injector, k);
	}
	schedule_us(50000, (ActivationFuncPtr) input_poller_update, ip);
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
