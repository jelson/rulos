/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
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

#include "periph/input_controller/input_controller.h"

#include "core/rulos.h"

void input_poller_update(InputPollerAct *ip);

void input_poller_init(InputPollerAct *ip, InputInjectorIfc *injector) {
  hal_init_keypad();
  ip->injector = injector;
  schedule_us(1, (ActivationFuncPtr)input_poller_update, ip);
}

void input_poller_update(InputPollerAct *ip) {
  char k = hal_read_keybuf();
  if (k != 0) {
    ip->injector->func(ip->injector, KeystrokeCtor(k));
  }
  schedule_us(50000, (ActivationFuncPtr)input_poller_update, ip);
}

//////////////////////////////////////////////////////////////////////////////

void input_focus_injector_deliver(InputFocusInjector *ifi, char k);

void input_focus_injector_init(InputFocusInjector *ifi,
                               UIEventHandler *topHandler) {
  ifi->func = (InputInjectorFunc)input_focus_injector_deliver;
  ifi->topHandler = topHandler;
  ifi->topHandlerActive = FALSE;
}

void input_focus_injector_deliver(InputFocusInjector *ifi, char k) {
  UIEventDisposition uied = uied_blur;
  if (ifi->topHandlerActive) {
    uied = ifi->topHandler->func(ifi->topHandler, k);
  } else if (k == uie_select || k == uie_right || k == uie_left) {
    uied = ifi->topHandler->func(ifi->topHandler, uie_focus);
  }
  switch (uied) {
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
