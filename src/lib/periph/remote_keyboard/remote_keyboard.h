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

#include "core/network.h"
#include "core/network_ports.h"
#include "periph/input_controller/input_controller.h"

typedef struct {
  Keystroke key;
} KeystrokeMessage;

typedef struct s_remote_keyboard_send {
  Network *network;
  Port port;
  uint8_t send_msg_alloc[sizeof(WireMessage) + sizeof(KeystrokeMessage)];
  SendSlot sendSlot;

  InputInjectorIfc forwardLocalStrokes;
  struct s_remote_keyboard_send *forward_this;
} RemoteKeyboardSend;

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network,
                               Addr addr, Port port);

typedef struct s_remote_keyboard_recv {
  uint8_t recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(KeystrokeMessage))];
  AppReceiver app_receiver;
  InputInjectorIfc *acceptNetStrokes;
} RemoteKeyboardRecv;

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network,
                               InputInjectorIfc *acceptNetStrokes, Port port);
