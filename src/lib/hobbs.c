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
		//LOGF((logfp, "hobbs hpam_set_port(%d)\n", FALSE));
		hpam_set_port(hobbs->hpam, hpam_hobbs, FALSE);
	}
	else if (evt==evt_idle_nowactive)
	{
		//LOGF((logfp, "hobbs hpam_set_port(%d)\n", TRUE));
		hpam_set_port(hobbs->hpam, hpam_hobbs, TRUE);
	}
	return uied_accepted;
}
