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

#include <stdint.h>
#include <stdlib.h>

#include "core/media.h"
#include "core/message.h"
#include "core/util.h"

#define PORT_NONE 255
#define SLOT_NONE 255
#define MAX_LISTENERS 10
#define SEND_QUEUE_SIZE 4
#define NET_MAX_PAYLOAD_SIZE 10

// We allocate num_receive_buffers of these things.
typedef struct {
  struct app_receiver_t *app_receiver;  // back pointer for access to user_data
  uint8_t payload_len;
  uint8_t data[0];
} MessageRecvBuffer;

typedef void (*RecvCompleteFunc)(MessageRecvBuffer *msg);

#define RECEIVE_BUFFER_SIZE(payload_capacity) \
  (sizeof(MessageRecvBuffer) + payload_capacity)
#define RECEIVE_RING_SIZE(num_receive_buffers, payload_capacity) \
  (RECEIVE_BUFFER_SIZE(payload_capacity) * num_receive_buffers)

typedef struct app_receiver_t {
  RecvCompleteFunc recv_complete_func;
  Port port;
  uint8_t payload_capacity;
  uint8_t num_receive_buffers;
  void *user_data;  // pointer can be used for user functions

  // message_recv_buffers should point to RECEIVE_RING_SIZE(...) bytes.
  uint8_t *message_recv_buffers;
} AppReceiver;

struct s_send_slot;
typedef void (*SendCompleteFunc)(struct s_send_slot *send_slot);
typedef struct s_send_slot {
  SendCompleteFunc func;
  Addr dest_addr;
  uint8_t payload_len;  // Size of application payload at msg->data
  WireMessage *wire_msg;
  bool sending;
  void *user_data;  // pointer can be used for user functions
} SendSlot, *SendSlotPtr;

#include "queue.mh"

QUEUE_DECLARE(SendSlotPtr)

typedef struct {
  AppReceiver *app_receivers[MAX_LISTENERS];
  uint8_t sendQueue_storage[sizeof(SendSlotPtrQueue) +
                            sizeof(SendSlotPtr) * SEND_QUEUE_SIZE];
  struct {
    MediaRecvSlot media_recv_slot;
    WireMessage wire_message;
    uint8_t payload[NET_MAX_PAYLOAD_SIZE];
  } media_recv_alloc;

  MediaStateIfc *media;
} Network;

// Public API
void init_network(Network *net);
void net_bind_media(Network *net, MediaStateIfc *media);
void net_bind_receiver(Network *net, AppReceiver *appReceiver);
void net_free_received_message_buffer(MessageRecvBuffer *msg);
bool net_send_message(Network *net, SendSlot *sendSlot);

//////////////////////////////////////////////////////////////////////////////

void init_twi_network(Network *network, uint32_t speed_khz, Addr local_addr);
