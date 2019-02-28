/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
