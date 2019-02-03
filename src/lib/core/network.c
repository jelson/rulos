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

#include "core/network.h"

#include "core/clock.h"
#include "core/hal.h"
#include "core/logging.h"
#include "core/net_compute_checksum.h"
#include "queue.mc"

QUEUE_DEFINE(SendSlotPtr)

#define SendQueue(n) ((SendSlotPtrQueue *)n->sendQueue_storage)

///////////////// Utilities ////////////////////////////////////

#if 0
void net_log_buffer(uint8_t *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		LOG("%02x ", buf[i]);
	}
	LOG("");
}
#endif

//////////// Network Init ////////////////////////////////////

static void net_recv_interrupt_handler(MediaRecvSlot *mrs);

void init_network(Network *net, MediaStateIfc *media) {
  for (uint8_t i = 0; i < MAX_LISTENERS; i++) {
    net->app_receivers[i] = NULL;
  }

  SendSlotPtrQueue_init(SendQueue(net), sizeof(net->sendQueue_storage));

  MediaRecvSlot *mrs = &net->media_recv_alloc.media_recv_slot;
  mrs->func = net_recv_interrupt_handler;
  mrs->capacity = sizeof(net->media_recv_alloc) - sizeof(MediaRecvSlot);
  mrs->packet_len = 0;
  mrs->user_data = net;

  // Set up underlying physical network
  net->media = media;
}

/////////////// Receiving /////////////////////////////////

void net_free_received_message_buffer(MessageRecvBuffer *msg) {
  // Mark buffer as unallocated.
  msg->payload_len = 0;
}

// Given a receiver, find a free message buffer in that receiver.
static MessageRecvBuffer *net_allocate_free_buffer(AppReceiver *app_receiver) {
  const uint8_t buf_size = RECEIVE_BUFFER_SIZE(app_receiver->payload_capacity);
  const uint8_t num_receive_buffers = app_receiver->num_receive_buffers;
  uint8_t idx;
  uint16_t offset;
  for (idx = 0, offset = 0; idx < num_receive_buffers;
       idx++, offset += buf_size) {
    MessageRecvBuffer *buffer =
        (MessageRecvBuffer *)(app_receiver->message_recv_buffers + offset);
    if (buffer->payload_len == 0) {
      LOG("XXX using ring buffer %d", idx);
      return buffer;
    }
  }
  return NULL;
}

// Find a registered listener by port number.
static AppReceiver *net_find_receiver(Network *net, Port port) {
  for (uint8_t i = 0; i < MAX_LISTENERS; i++) {
    if (net->app_receivers[i] == NULL) {
      continue;
    }
    if (net->app_receivers[i]->port == port) {
      return net->app_receivers[i];
    }
  }
  return NULL;
}

// Where to register a new listener.
static int net_find_empty_receive_slot(Network *net) {
  for (uint8_t i = 0; i < MAX_LISTENERS; i++) {
    if (net->app_receivers[i] == NULL) {
      return i;
    }
  }
  return SLOT_NONE;
}

// Public API function to start listening on a port
void net_bind_receiver(Network *net, AppReceiver *app_receiver) {
  assert(app_receiver != NULL);
  assert(app_receiver->recv_complete_func != NULL);
  assert(app_receiver->port > 0);
  assert(app_receiver->num_receive_buffers > 0);
  assert(app_receiver->payload_capacity > 0);
  assert(app_receiver->message_recv_buffers != NULL);
  uint8_t slotIdx = net_find_empty_receive_slot(net);
  assert(slotIdx != SLOT_NONE);
  net->app_receivers[slotIdx] = app_receiver;

  // Initialize all the buffers in the provided buffer space.
  const uint8_t buf_size = RECEIVE_BUFFER_SIZE(app_receiver->payload_capacity);
  uint8_t idx;
  uint16_t offset;
  for (idx = 0, offset = 0; idx < app_receiver->num_receive_buffers;
       idx++, offset += buf_size) {
    MessageRecvBuffer *buffer =
        (MessageRecvBuffer *)(app_receiver->message_recv_buffers + offset);
    buffer->app_receiver = app_receiver;  // provide back pointer to user data
    buffer->payload_len = 0;              // mark unoccupied
  }

  LOG("netstack: listener bound to port %d (0x%x), slot %d", app_receiver->port,
      app_receiver->port, slotIdx);
}

// Called by the media layer when a packet arrives in our receive slot.
// WARNING: Runs on interrupt stack! Some sort of locking discipline required.
// Returning from this function releases access to mrs, so better copy out
// the packet before we leave.
static void net_recv_interrupt_handler(MediaRecvSlot *mrs) {
  Network *const net = (Network *)mrs->user_data;
  WireMessage *const wire_msg = (WireMessage *)mrs->data;
  const uint8_t packet_len = mrs->packet_len;
  if (packet_len < sizeof(WireMessage)) {
    LOG("netstack error: got short pkt only %d bytes", packet_len);
    return;
  }

  // Make sure the checksum matches. We do this on the interrupt stack
  // because it covers data (the port number) that we need now and that
  // we'll discard before transferring to the scheduler stack.
  uint8_t incoming_checksum = wire_msg->checksum;
  wire_msg->checksum = 0;
  if (net_compute_checksum((unsigned char *)wire_msg, packet_len) !=
      incoming_checksum) {
    LOG("netstack error: checksum mismatch");
    return;
  }

  // Now we know the port is valid; find the receiver.
  AppReceiver *app_receiver = net_find_receiver(net, wire_msg->dest_port);
  if (app_receiver == NULL) {
    LOG("netstack error: dropped packet to port %d (0x%x) with no listener",
        wire_msg->dest_port, wire_msg->dest_port);
    return;
  }

  const uint8_t payload_len = packet_len - sizeof(WireMessage);
  if (payload_len == 0) {
    // Hey, that's our unused-buffer signal!
    LOG("netstack error: dropping zero-length payload (port %d).",
        wire_msg->dest_port);
    return;
  }

  if (payload_len > app_receiver->payload_capacity) {
    LOG("netstack error: dropped packet of length %d (port 0x%x), exceeds "
        "receiver capacity",
        payload_len, wire_msg->dest_port);
    return;
  }

  // Find a buffer to copy into from the MediaRecvSlot.
  MessageRecvBuffer *recv_buffer = net_allocate_free_buffer(app_receiver);
  if (recv_buffer == NULL) {
    LOG("netstack error: dropped packet of length %d (port 0x%x); ring full",
        payload_len, wire_msg->dest_port);
    return;
  }
  recv_buffer->payload_len = payload_len;
  memcpy(recv_buffer->data, mrs->data + sizeof(WireMessage), mrs->packet_len);

  // Schedule the upcall.
  schedule_now((ActivationFuncPtr)app_receiver->recv_complete_func,
               recv_buffer);
}

/////////////// Sending ///////////////////////////////////////////////

static void net_send_next_message_down(Network *net);
static void net_send_done_cb(void *user_data);

// External visible API from above to launch a packet.
r_bool net_send_message(Network *net, SendSlot *sendSlot) {
#if 0
  LOG("netstack: queueing %d-byte payload to %d:%d (0x%x:0x%x)",
      sendSlot->msg->payload_len, sendSlot->dest_addr, sendSlot->msg->dest_port,
      sendSlot->dest_addr, sendSlot->msg->dest_port);
#endif

  assert(sendSlot->sending == FALSE);
  r_bool need_wake = (SendSlotPtrQueue_length(SendQueue(net)) == 0);
  r_bool fit = SendSlotPtrQueue_append(SendQueue(net), sendSlot);

  if (fit) {
    sendSlot->sending = TRUE;
  }
  if (need_wake) {
    net_send_next_message_down(net);
  }
  return fit;
}

// Send a message down the stack.
static void net_send_next_message_down(Network *net) {
  // Get the next sendSlot out of our queue.  Return if none.
  SendSlot *sendSlot;
  r_bool rc = SendSlotPtrQueue_peek(SendQueue(net), &sendSlot);

  if (rc == FALSE) {
    return;
  }

#if 0
  LOG("netstack: releasing %d-byte payload to %d:%d (0x%x:0x%x)",
      sendSlot->msg->payload_len, sendSlot->dest_addr, sendSlot->msg->dest_port,
      sendSlot->dest_addr, sendSlot->msg->dest_port);
#endif

  // Make sure this guy thinks he's sending.
  assert(sendSlot->sending == TRUE);

  // compute length and checksum.
  uint8_t packet_len = sizeof(WireMessage) + sendSlot->payload_len;
  assert(packet_len > 0);
  sendSlot->wire_msg->checksum = 0;
  sendSlot->wire_msg->checksum =
      net_compute_checksum((unsigned char *)sendSlot->wire_msg, packet_len);

  // send down
  (net->media->send)(net->media, sendSlot->dest_addr,
                     (unsigned char *)sendSlot->wire_msg, packet_len,
                     net_send_done_cb, net);
}

// Called when media has finished sending down the stack.
// Mark the SendSlot's buffer as free and call the callback.
static void net_send_done_cb(void *user_data) {
  Network *net = (Network *)user_data;
  SendSlot *sendSlot;
  r_bool rc = SendSlotPtrQueue_pop(SendQueue(net), &sendSlot);

  assert(rc);
  assert(sendSlot->sending == TRUE);

  // mark the packet as no-longer-sending and call the user's
  // callback
  sendSlot->sending = FALSE;
  if (sendSlot->func) sendSlot->func(sendSlot);

  // launch the next queued packet if any
  net_send_next_message_down(net);
}

//////////////////////////////////////////////////////////////////////////////

void init_twi_network(Network *net, uint32_t speed_khz, Addr local_addr) {
  init_network(net, hal_twi_init(speed_khz, local_addr,
                                 &net->media_recv_alloc.media_recv_slot));
}
