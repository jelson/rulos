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

#include "periph/remote_keyboard/remote_keyboard.h"

void rk_send(InputInjectorIfc *injector, char key);
void rk_send_complete(SendSlot *sendSlot);
void rk_recv(RecvSlot *recvSlot, uint8_t payload_len);

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network,
                               Addr addr, Port port) {
  rk->network = network;
  rk->port = port;

  rk->sendSlot.func = NULL;
  rk->sendSlot.msg = (Message *)rk->send_msg_alloc;
  rk->sendSlot.dest_addr = addr;
  rk->sendSlot.msg->dest_port = rk->port;
  rk->sendSlot.msg->payload_len = sizeof(KeystrokeMessage);
  rk->sendSlot.sending = FALSE;

  rk->forwardLocalStrokes.func = rk_send;
  rk->forward_this = rk;
}

void rk_send(InputInjectorIfc *injector, char key) {
  RemoteKeyboardSend *rk = *(RemoteKeyboardSend **)(injector + 1);
  KeystrokeMessage *km = (KeystrokeMessage *)&rk->sendSlot.msg->data;

  if (rk->sendSlot.sending) {
    LOG("RemoteKeyboard drops a message due to full send queue.\n");
    return;
  }

  km->key = key;
  net_send_message(rk->network, &rk->sendSlot);
}

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network,
                               InputInjectorIfc *acceptNetStrokes, Port port) {
  rk->recvSlot.func = rk_recv;
  rk->recvSlot.port = port;
  rk->recvSlot.payload_capacity = sizeof(KeystrokeMessage);
  rk->recvSlot.msg_occupied = FALSE;
  rk->recvSlot.msg = (Message *)rk->recv_msg_alloc;
  rk->recvSlot.user_data = rk;

  rk->acceptNetStrokes = acceptNetStrokes;

  net_bind_receiver(network, &rk->recvSlot);
}

void rk_recv(RecvSlot *recvSlot, uint8_t payload_len) {
  RemoteKeyboardRecv *rk = (RemoteKeyboardRecv *)recvSlot->user_data;
  KeystrokeMessage *km = (KeystrokeMessage *)recvSlot->msg->data;
  assert(payload_len == sizeof(KeystrokeMessage));
  LOG("remote key: %c\n", km->key);
  rk->acceptNetStrokes->func(rk->acceptNetStrokes, km->key);
  recvSlot->msg_occupied = FALSE;
}
