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

#pragma once

#include "core/rulos.h"

struct s_focus_handler;
typedef uint8_t UIEvent;

typedef enum {
  uie_focus = 0x81,
  uie_select = 'c',
  uie_escape = 'd',
  uie_right = 'a',
  uie_left = 'b',
  evt_notify = 0x80,
  evt_remote_escape = 0x82,
  evt_idle_nowidle = 0x83,
  evt_idle_nowactive = 0x84,
} EventName;

typedef enum {
  uied_accepted,  // child consumed event
  uied_blur       // child releases focus
} UIEventDisposition;

typedef UIEventDisposition (*UIEventHandlerFunc)(
    struct s_focus_handler *handler, UIEvent evt);
typedef struct s_focus_handler {
  UIEventHandlerFunc func;
} UIEventHandler;
