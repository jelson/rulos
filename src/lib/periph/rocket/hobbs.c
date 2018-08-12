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

#include "hobbs.h"

UIEventDisposition hobbs_handler(Hobbs *hobbs, UIEvent evt);

void init_hobbs(Hobbs *hobbs, HPAM *hpam, IdleAct *idle)
{
	hobbs->func = (UIEventHandlerFunc) hobbs_handler;
	hobbs->hpam = hpam;
	idle_add_handler(idle, (UIEventHandler *) hobbs);
}

UIEventDisposition hobbs_handler(Hobbs *hobbs, UIEvent evt)
{
	if (evt==evt_idle_nowidle)
	{
		//LOG("hobbs hpam_set_port(%d)\n", FALSE);
		hpam_set_port(hobbs->hpam, hpam_hobbs, FALSE);
	}
	else if (evt==evt_idle_nowactive)
	{
		//LOG("hobbs hpam_set_port(%d)\n", TRUE);
		hpam_set_port(hobbs->hpam, hpam_hobbs, TRUE);
	}
	return uied_accepted;
}
