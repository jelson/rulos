#ifndef __input_controller_h__
#define __input_controller_h__

struct s_input_injector_ifc;
typedef void (*InputInjectorFunc)(struct s_input_injector_ifc *ii, char key);
typedef struct s_input_injector_ifc {
	InputInjectorFunc func;
} InputInjectorIfc;

//////////////////////////////////////////////////////////////////////////////
// Poll hardware for keys
//////////////////////////////////////////////////////////////////////////////

typedef struct {
	ActivationFunc func;
	InputInjectorIfc *injector;
} InputPollerAct;

void input_poller_init(InputPollerAct *ip, InputInjectorIfc *injector);

//////////////////////////////////////////////////////////////////////////////
// Deliver keys to the root of a focus tree.
//////////////////////////////////////////////////////////////////////////////

typedef struct {
	InputInjectorFunc func;
	UIEventHandler *topHandler;
	uint8_t topHandlerActive;
} InputFocusInjector;

void input_focus_injector_init(
	InputFocusInjector *ifi, UIEventHandler *topHandler);

#endif // input_controller_h

