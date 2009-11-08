#ifndef _remote_uie_h
#define _remote_uie_h

#include "rocket.h"
#include "focus.h"
#include "input_controller.h"

typedef struct {
	UIEventHandlerFunc func;
	InputInjectorIfc *iii;
} RemoteUIE;

void init_remote_uie(RemoteUIE *ruie, InputInjectorIfc *iii);

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	InputInjectorFunc func;
	UIEventHandler *uie_handler;
	InputInjectorIfc *escape_ifi;
} CascadedInputInjector;

void init_cascaded_input_injector(
	CascadedInputInjector *cii,
	UIEventHandler *uie_handler,
	InputInjectorIfc *escape_ifi);

#endif // _remote_uie_h
