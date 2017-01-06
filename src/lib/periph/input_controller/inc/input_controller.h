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

#pragma once

#include "focus.h"

struct s_input_injector_ifc;
typedef void (*InputInjectorFunc)(struct s_input_injector_ifc *ii, char key);
typedef struct s_input_injector_ifc {
	InputInjectorFunc func;
} InputInjectorIfc;

//////////////////////////////////////////////////////////////////////////////
// Poll hardware for keys
//////////////////////////////////////////////////////////////////////////////

typedef struct {
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

