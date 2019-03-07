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

#define REMOTE_BBUF_RING_SIZE 4
#define REMOTE_BBUF_RING_NEXT(x) ((x + 1) % RING_SIZE)
// Thread context for rbs_update and rbs_refresh.
#define RING_INVALID_INDEX (255)

typedef struct s_remote_bbuf_recv {
  uint8_t recv_ring_alloc[RECEIVE_RING_SIZE(REMOTE_BBUF_RING_SIZE,
                                            sizeof(BBufMessage))];
  AppReceiver app_receiver;
} RemoteBBufRecv;

// Initialize the remote bbuf service.
void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network);
