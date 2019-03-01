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

#include "core/hal.h"
#include "core/media.h"
#include "periph/uart/uart_net_media_preamble.h"

struct s_UartMedia;

typedef struct {
  const unsigned char *data;
  uint8_t len;
  MediaSendDoneFunc sendDoneCB;
  void *sendDoneCBData;
} UartMediaSendingPayload;

typedef struct {
  MediaStateIfc media;
  struct s_UartMedia *uart_media;
} UartMediaPtr;

typedef enum { US_none = 17, US_preamble, US_packet } UartMedia_SendState;

typedef enum {
  UR_sync0,
  UR_sync1,
  UR_addr,
  UR_len,
  UR_payload,
} UartMedia_RecvState;

typedef struct s_UartMedia {
  HalUart uart_handler;
  UartMediaPtr uart_media_ptr;

  uint8_t send_which;
  uint8_t send_dataidx;
  UartPreamble sending_preamble;
  UartMediaSendingPayload sending_payload;

  MediaRecvSlot *mrs;
  uint8_t recv_state;
  uint8_t recv_addr;  // TODO discarding addr info; will need for forwarding.
  uint8_t recv_len;
  uint8_t recv_dataidx;
} UartMedia;

// Strategy: mimic TWI interface, with uart media underneath.
// (Think PPP.)
// Then another 'multihost' layer will multiplex between the media
// on the audio board, and maybe even do packet forwarding. Tee hee.

MediaStateIfc *uart_media_init(UartMedia *um, MediaRecvSlot *mrs,
                               uint8_t uart_id);
