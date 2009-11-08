#include "remote_uie.h"

UIEventDisposition remote_uie_handler(RemoteUIE *ruie, UIEvent evt);

void init_remote_uie(RemoteUIE *ruie, InputInjectorIfc *iii)
{
	ruie->func = (UIEventHandlerFunc) remote_uie_handler;
	ruie->iii = iii;
}

UIEventDisposition remote_uie_handler(RemoteUIE *ruie, UIEvent evt)
{
	ruie->iii->func(ruie->iii, evt);
	return uied_accepted;
}

//////////////////////////////////////////////////////////////////////////////

void cii_deliver(CascadedInputInjector *cii, char k);

void init_cascaded_input_injector(
	CascadedInputInjector *cii,
	UIEventHandler *uie_handler,
	InputInjectorIfc *escape_ifi)
{
	cii->func = (InputInjectorFunc) cii_deliver;
	cii->uie_handler = uie_handler;
	cii->escape_ifi = escape_ifi;
}

void cii_deliver(CascadedInputInjector *cii, char k)
{
	UIEventDisposition uied = cii->uie_handler->func(cii->uie_handler, k);
	if (uied==uied_blur)
	{
		// my handler wants to pass focus back to remote end.
		// We'll hack that for now by synthesizing an "escape" keypress.
		// This hack assumes deep knowledge of the layout of trees, boards,
		// and keyboards in this rocket; it won't generalize well to
		// ... um ... future rockets.
		cii->escape_ifi->func(cii->escape_ifi, evt_remote_escape);
	}
}
