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
#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/display_controller.h"

typedef struct {
  SSBitmap buf[NUM_DIGITS];
  uint8_t index;
} BBufMessage;

typedef struct s_remote_bbuf_send {
  Network *network;
  uint8_t send_msg_alloc[sizeof(WireMessage) + sizeof(BBufMessage)];
  SendSlot sendSlot;
  struct s_remote_bbuf_send *send_this;
  SSBitmap offscreen[NUM_REMOTE_BOARDS][NUM_DIGITS];
  r_bool changed[NUM_REMOTE_BOARDS];
  uint8_t last_index;
} RemoteBBufSend;

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network);

// Inbound interface from callsite in board_buffer that delivers remote boards.
void send_remote_bbuf(RemoteBBufSend *rbs, SSBitmap *bm, uint8_t index,
                      uint8_t mask);

#define RING_SIZE 4
#define RING_NEXT(x) ((x + 1) % RING_SIZE)
// Thread context for rbs_update and rbs_refresh.
#define RING_INVALID_INDEX (255)

typedef struct s_remote_bbuf_recv {
  uint8_t recv_ring_alloc[RECEIVE_RING_SIZE(RING_SIZE, sizeof(BBufMessage))];
  AppReceiver app_receiver;
} RemoteBBufRecv;

// Initialize the remote bbuf service.
void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network);
