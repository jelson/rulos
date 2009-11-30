#include "thruster_protocol.h"

void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload);
void tsn_timer_func(ThrusterUpdateTimer *tut);
void thruster_message_sent(SendSlot *sendSlot);

void init_thruster_send_network(ThrusterSendNetwork *tsn, Network *network)
{
	tsn->func = (ThrusterUpdateFunc) tsn_update;
	tsn->network = network;

	tsn->sendSlot.func = thruster_message_sent;
	tsn->sendSlot.msg = (Message*) tsn->thruster_message_storage;
	tsn->sendSlot.msg->dest_port = THRUSTER_PORT;
	tsn->sendSlot.msg->message_size = sizeof(ThrusterPayload);
	tsn->sendSlot.sending = FALSE;

	tsn->last_state.thruster_bits = 0;
	tsn->state_changed = TRUE;
	tsn->timer.func = (ActivationFunc) tsn_timer_func;
	tsn->timer.tsn = tsn;

	schedule_us(1, (Activation*) &tsn->timer);
}

// To be sure messages are eventually sent (even if they're delayed
// to avoid being too chatty, or due to a busy send buffer), we
// do a periodic send check all the time. If it were easy to racelessly
// start/stop periodic tasks as needed, we would, but it's not, so we
// just waste a few cycles on the scheduler heap.

void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload)
{
	if (payload->thruster_bits != tsn->last_state.thruster_bits)
	{
		tsn->last_state.thruster_bits = payload->thruster_bits;
		tsn->state_changed = TRUE;
	}
}

void tsn_timer_func(ThrusterUpdateTimer *tut)
{
	ThrusterSendNetwork *tsn = tut->tsn;
	schedule_us(1000000/20, (Activation*) &tsn->timer);

	if (tsn->state_changed && !tsn->sendSlot.sending)
	{
		ThrusterPayload *tp = (ThrusterPayload *) tsn->sendSlot.msg->data;
		tp->thruster_bits = tsn->last_state.thruster_bits;
		net_send_message(tsn->network, &tsn->sendSlot);
		tsn->state_changed = FALSE;
	}
}

void thruster_message_sent(SendSlot *sendSlot)
{
	sendSlot->sending = FALSE;
}
