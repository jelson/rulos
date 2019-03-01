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

#include "periph/rocket/hobbs.h"

UIEventDisposition hobbs_handler(Hobbs *hobbs, UIEvent evt);

void init_hobbs(Hobbs *hobbs, HPAM *hpam, IdleAct *idle) {
  hobbs->func = (UIEventHandlerFunc)hobbs_handler;
  hobbs->hpam = hpam;
  idle_add_handler(idle, (UIEventHandler *)hobbs);
}

UIEventDisposition hobbs_handler(Hobbs *hobbs, UIEvent evt) {
  if (evt == evt_idle_nowidle) {
    // LOG("hobbs hpam_set_port(%d)", FALSE);
    hpam_set_port(hobbs->hpam, HPAM_HOBBS, FALSE);
  } else if (evt == evt_idle_nowactive) {
    // LOG("hobbs hpam_set_port(%d)", TRUE);
    hpam_set_port(hobbs->hpam, HPAM_HOBBS, TRUE);
  }
  return uied_accepted;
}
