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
  uint8_t thruster_message_storage[sizeof(Message) + sizeof(ThrusterPayload)];
  SendSlot sendSlot;
  ThrusterPayload last_state;
  r_bool state_changed;
} ThrusterSendNetwork;

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network);
void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload);
