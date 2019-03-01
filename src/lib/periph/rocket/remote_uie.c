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

#include "periph/rocket/remote_uie.h"

UIEventDisposition remote_uie_handler(RemoteUIE *ruie, UIEvent evt);

void init_remote_uie(RemoteUIE *ruie, InputInjectorIfc *iii) {
  ruie->func = (UIEventHandlerFunc)remote_uie_handler;
  ruie->iii = iii;
}

UIEventDisposition remote_uie_handler(RemoteUIE *ruie, UIEvent evt) {
  ruie->iii->func(ruie->iii, KeystrokeCtor(evt));
  return uied_accepted;
}

//////////////////////////////////////////////////////////////////////////////

void cii_deliver(CascadedInputInjector *cii, char k);

void init_cascaded_input_injector(CascadedInputInjector *cii,
                                  UIEventHandler *uie_handler,
                                  InputInjectorIfc *escape_ifi) {
  cii->func = (InputInjectorFunc)cii_deliver;
  cii->uie_handler = uie_handler;
  cii->escape_ifi = escape_ifi;
}

void cii_deliver(CascadedInputInjector *cii, char k) {
  UIEventDisposition uied = cii->uie_handler->func(cii->uie_handler, k);
  if (uied == uied_blur) {
    // my handler wants to pass focus back to remote end.
    // We'll hack that for now by synthesizing an "escape" keypress.
    // This hack assumes deep knowledge of the layout of trees, boards,
    // and keyboards in this rocket; it won't generalize well to
    // ... um ... future rockets.
    cii->escape_ifi->func(cii->escape_ifi, KeystrokeCtor(evt_remote_escape));
  }
}
