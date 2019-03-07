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

#include <string.h>

#include "core/clock.h"
#include "core/rulos.h"
#include "periph/7seg_panel/remote_bbuf.h"

//////////////////////////////////////////////////////////////////////////////
// Send side

#define REMOTE_BBUF_SEND_RATE 10000            // 100 board msgs per sec
#define REMOTE_BBUF_REFRESH_PERIOD_US 1000000  // 1s refresh rate

static void rbs_update(RemoteBBufSend *rbs);
void rbs_refresh(RemoteBBufSend *rbs);
void rbs_send_complete(SendSlot *slot);

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network) {
  rbs->network = network;
  rbs->sendSlot.func = rbs_send_complete;
  rbs->sendSlot.wire_msg = (WireMessage *)rbs->send_msg_alloc;
  rbs->sendSlot.sending = FALSE;
  rbs->sendSlot.user_data = rbs;
#if NUM_REMOTE_BOARDS > 0
  memset(rbs->offscreen, 0, NUM_REMOTE_BOARDS * NUM_DIGITS * sizeof(SSBitmap));
#endif
  memset(rbs->changed, FALSE, NUM_REMOTE_BOARDS * sizeof(r_bool));
  rbs->last_index = 0;

  schedule_us(1, (ActivationFuncPtr)rbs_update, rbs);
  schedule_us(1, (ActivationFuncPtr)rbs_refresh, rbs);
}

void send_remote_bbuf(RemoteBBufSend *rbs, SSBitmap *bm, uint8_t index,
                      uint8_t mask) {
  assert(0 <= index && index < NUM_REMOTE_BOARDS);

  for (int di = 0; di < NUM_DIGITS; di++) {
    if (mask & 0x80) {
      rbs->offscreen[index][di] = bm[di];
    }
    mask <<= 1;
  }
  rbs->changed[index] = TRUE;
}

// Finds the next changed board since we last sent a board.
int rbs_find_changed_index(RemoteBBufSend *rbs) {
#if NUM_REMOTE_BOARDS > 0
  int idx = rbs->last_index;
  int tries;
  for (tries = 0; tries < NUM_REMOTE_BOARDS; tries++) {
    idx = (idx + 1) % NUM_REMOTE_BOARDS;
    if (rbs->changed[idx]) {
      return idx;
    }
  }
#endif
  return -1;
}

// Extract the remote address mappings from the DBOARD definitions into a
// static.
#define DBOARD(name, syms, x, y, remote_addr, remote_idx) \
  { remote_addr, remote_idx }
#define B_NO_BOARD /**/
#define B_END      /**/
#include "periph/7seg_panel/display_tree.ch"

typedef struct {
  uint8_t remote_addr;
  uint8_t remote_index;
} RemoteMapping;
static const RemoteMapping remote_mapping[] = {ROCKET_TREE};

int remote_mapping_count = sizeof(remote_mapping) / sizeof(RemoteMapping);

void rbs_send_complete(SendSlot *sendSlot) {
  RemoteBBufSend *rbs = (RemoteBBufSend *)sendSlot->user_data;
  rbs_update(rbs);
}

// This thread looks for a board that has been changed since we last sent it,
// and transmits it. It finds boards round-robin to avoid starvation. It runs
// on a regular schedule avoid swamping the network with board traffic.
void rbs_update(RemoteBBufSend *rbs) {
  assert(!rbs->sendSlot.sending);

  int index = rbs_find_changed_index(rbs);
  if (index == -1) {
    // no changed lines; try again later.
#if BBDEBUG
    LOG("rbs_update: idle");
#endif
    schedule_us(REMOTE_BBUF_SEND_RATE, (ActivationFuncPtr)rbs_update, rbs);
    return;
  }

#if BBDEBUG
  LOG("rbs_update: update[%d]", index);
#endif

  // send a packet for this changed line
  const RemoteMapping *mapping = &remote_mapping[index];
  rbs->sendSlot.dest_addr = DONGLE_BASE_ADDR + mapping->remote_addr;
  rbs->sendSlot.wire_msg->dest_port = REMOTE_BBUF_PORT;
  rbs->sendSlot.payload_len = sizeof(BBufMessage);
  BBufMessage *bbm = (BBufMessage *)&rbs->sendSlot.wire_msg->data;
  memcpy(bbm->buf, rbs->offscreen[index], NUM_DIGITS);
  bbm->index = mapping->remote_index;

  if (net_send_message(rbs->network, &rbs->sendSlot)) {
    rbs->changed[index] = FALSE;
  }

  rbs->last_index = index;
}

// This thread periodically marks all boards changed to ensure that dropped
// messages are eventually cleaned up.
void rbs_refresh(RemoteBBufSend *rbs) {
  schedule_us(REMOTE_BBUF_REFRESH_PERIOD_US, (ActivationFuncPtr)rbs_refresh,
              rbs);
#if NUM_REMOTE_BOARDS > 0
  int idx;
  for (idx = 0; idx < NUM_REMOTE_BOARDS; idx++) {
    rbs->changed[idx] = TRUE;
  }
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Receive side
// TODO we should probably put this in a separate file, huh?

void rbr_recv(MessageRecvBuffer *msg);

void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network) {
  rbr->app_receiver.recv_complete_func = rbr_recv;
  rbr->app_receiver.port = REMOTE_BBUF_PORT;
  rbr->app_receiver.num_receive_buffers = REMOTE_BBUF_RING_SIZE;
  rbr->app_receiver.payload_capacity = sizeof(BBufMessage);
  rbr->app_receiver.message_recv_buffers = rbr->recv_ring_alloc;

  net_bind_receiver(network, &rbr->app_receiver);
}

void rbr_recv(MessageRecvBuffer *msg) {
  if (msg->payload_len != sizeof(BBufMessage)) {
    LOG("rbr_recv: Error: expected BBufMessage of size %ld, got %d bytes",
        sizeof(BBufMessage), msg->payload_len);
    return;
  }

  BBufMessage *bbm = (BBufMessage *)msg->data;

#if BBDEBUG
  LOG("rbs: updating board %d with data %x%x%x%x%x%x%x%x", bbm->index,
      bbm->buf[0], bbm->buf[1], bbm->buf[2], bbm->buf[3], bbm->buf[4],
      bbm->buf[5], bbm->buf[6], bbm->buf[7]);
#endif

  board_buffer_paint(bbm->buf, bbm->index, 0xff);

  net_free_received_message_buffer(msg);
}
