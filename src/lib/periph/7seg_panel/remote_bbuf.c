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

#include <string.h>

#include "core/clock.h"
#include "core/rulos.h"
#include "periph/7seg_panel/remote_bbuf.h"

#define REMOTE_BBUF_SEND_RATE 10000            // 100 board msgs per sec
#define REMOTE_BBUF_REFRESH_PERIOD_US 1000000  // 1s refresh rate

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network) {
  rbs->network = network;
  rbs->sendSlot.func = NULL;
  rbs->sendSlot.msg = (Message *)rbs->send_msg_alloc;
  rbs->sendSlot.sending = FALSE;
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

// This thread looks for a board that has been changed since we last sent it,
// and transmits it. It finds boards round-robin to avoid starvation. It runs
// on a regular schedule avoid swamping the network with board traffic.
void rbs_update(RemoteBBufSend *rbs) {
  schedule_us(REMOTE_BBUF_SEND_RATE, (ActivationFuncPtr)rbs_update, rbs);

  if (rbs->sendSlot.sending) {
    LOG("rbs_update: dropping update; sender busy\n");
    return;
  }

  int index = rbs_find_changed_index(rbs);
  if (index == -1) {
#if BBDEBUG
    // no changed lines
    LOG("rbs_update: idle\n");
#endif
    return;
  }

#if BBDEBUG
  LOG("rbs_update: update[%d]\n", index);
#endif

  // send a packet for this changed line
  const RemoteMapping *mapping = &remote_mapping[index];
  rbs->sendSlot.dest_addr = DONGLE_BASE_ADDR + mapping->remote_addr;
  rbs->sendSlot.msg->dest_port = REMOTE_BBUF_PORT;
  rbs->sendSlot.msg->payload_len = sizeof(BBufMessage);
  BBufMessage *bbm = (BBufMessage *)&rbs->sendSlot.msg->data;
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

void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network) {
  rbr->recvSlot.func = rbr_recv;
  rbr->recvSlot.port = REMOTE_BBUF_PORT;
  rbr->recvSlot.payload_capacity = sizeof(BBufMessage);
  rbr->recvSlot.msg_occupied = FALSE;
  rbr->recvSlot.msg = (Message *)rbr->recv_msg_alloc;

  net_bind_receiver(network, &rbr->recvSlot);
}

void rbr_recv(RecvSlot *recvSlot, uint8_t payload_len) {
  if (payload_len != sizeof(BBufMessage)) {
    LOG("Error: expected BBufMessage of size %ld, got %d bytes",
        sizeof(BBufMessage), payload_len);
    return;
  }

  BBufMessage *bbm = (BBufMessage *)&recvSlot->msg->data;

#if BBDEBUG
  LOG("rbs: updating board %d with data %x%x%x%x%x%x%x%x\n", bbm->index,
      bbm->buf[0], bbm->buf[1], bbm->buf[2], bbm->buf[3], bbm->buf[4],
      bbm->buf[5], bbm->buf[6], bbm->buf[7]);
#endif

  board_buffer_paint(bbm->buf, bbm->index, 0xff);
  recvSlot->msg_occupied = FALSE;
}
