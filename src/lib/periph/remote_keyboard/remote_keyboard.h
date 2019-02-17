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
