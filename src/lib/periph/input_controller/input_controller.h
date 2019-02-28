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

#include "periph/input_controller/ui_event_handler_ifc.h"

typedef struct Keystroke {
    unsigned char key;
} Keystroke;

static inline Keystroke KeystrokeCtor(unsigned char c) {
    Keystroke k = {c};
    return k;
}

static inline bool KeystrokeCmp(Keystroke k1, Keystroke k2) {
    return k1.key == k2.key;
}

struct s_input_injector_ifc;
typedef void (*InputInjectorFunc)(struct s_input_injector_ifc *ii, Keystroke key);
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

void input_focus_injector_init(InputFocusInjector *ifi,
                               UIEventHandler *topHandler);
