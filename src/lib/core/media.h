/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

typedef uint8_t Addr;

struct s_MediaRecvSlot;
typedef void (*MediaRecvDispatcher)(struct s_MediaRecvSlot *recvSlot);
typedef struct s_MediaRecvSlot {
  MediaRecvDispatcher func;
  uint8_t capacity;
  uint8_t packet_len;  // 0 tells hardware handler this slot is empty.
  void *user_data;     // storage for a pointer back to your state structure
  char data[0];
} MediaRecvSlot;

typedef void (*MediaSendDoneFunc)(void *user_data);
typedef struct s_MediaStateIfc {
  void (*send)(struct s_MediaStateIfc *media, Addr dest_addr,
               const unsigned char *data, uint8_t len,
               MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
} MediaStateIfc;
