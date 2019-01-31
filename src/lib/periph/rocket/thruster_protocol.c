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

#include "periph/rocket/rocket.h"

void tsn_timer_func(ThrusterSendNetwork *tsn);
void thruster_message_sent(SendSlot *sendSlot);

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network) {
  tsn->network = network;

  tsn->sendSlot.func = NULL;
  tsn->sendSlot.msg = (Message *)tsn->thruster_message_storage;
  tsn->sendSlot.dest_addr = ROCKET1_ADDR;
  tsn->sendSlot.msg->dest_port = THRUSTER_PORT;
  tsn->sendSlot.msg->payload_len = sizeof(ThrusterPayload);
  tsn->sendSlot.sending = FALSE;

  tsn->last_state.thruster_bits = 0;
  tsn->state_changed = TRUE;

  schedule_us(1, (ActivationFuncPtr)tsn_timer_func, tsn);
}

// To be sure messages are eventually sent (even if they're delayed
// to avoid being too chatty, or due to a busy send buffer), we
// do a periodic send check all the time. If it were easy to racelessly
// start/stop periodic tasks as needed, we would, but it's not, so we
// just waste a few cycles on the scheduler heap.

void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload) {
  if (payload->thruster_bits != tsn->last_state.thruster_bits) {
    tsn->last_state.thruster_bits = payload->thruster_bits;
    tsn->state_changed = TRUE;
  }
}

void tsn_timer_func(ThrusterSendNetwork *tsn) {
  schedule_us(1000000 / 20, (ActivationFuncPtr)tsn_timer_func, tsn);

  if (tsn->state_changed && !tsn->sendSlot.sending) {
    // LOG("tsn: state change, sending packet");

    ThrusterPayload *tp = (ThrusterPayload *)tsn->sendSlot.msg->data;
    tp->thruster_bits = tsn->last_state.thruster_bits;

    if (net_send_message(tsn->network, &tsn->sendSlot)) {
      tsn->state_changed = FALSE;
    }
  }
}
