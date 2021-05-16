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

#define THRUSTER_REPORT_INTERVAL 250000

typedef struct {
  uint8_t thruster_bits;
} ThrusterPayload;

typedef void (*ThrusterUpdateFunc)(void *data, ThrusterPayload *payload);

// Record of someone who wants to be notified when thurster status changes
typedef struct {
  ThrusterUpdateFunc func;
  void *data;
} ThrusterUpdate;

typedef struct s_thruster_send_network {
  Network *network;
  uint8_t
      thruster_message_storage[sizeof(WireMessage) + sizeof(ThrusterPayload)];
  SendSlot sendSlot;
  ThrusterPayload last_state;
  bool state_changed;
} ThrusterSendNetwork;

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network);
void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload);
