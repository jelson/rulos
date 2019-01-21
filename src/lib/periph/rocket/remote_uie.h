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

#include "periph/input_controller/focus.h"
#include "periph/input_controller/input_controller.h"
#include "periph/rocket/rocket.h"

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

void init_cascaded_input_injector(CascadedInputInjector *cii,
                                  UIEventHandler *uie_handler,
                                  InputInjectorIfc *escape_ifi);
