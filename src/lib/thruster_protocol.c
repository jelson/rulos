#include "thruster_protocol.h"

void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload);
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

	tsn->last_send = clock_time_us();
}

void tsn_update(ThrusterSendNetwork *tsn, ThrusterPayload *payload)
{
	if (later_than(clock_time_us(), tsn->last_send + THRUSTER_REPORT_INTERVAL)
		&& !tsn->sendSlot.sending)
	{
		ThrusterPayload *tp = (ThrusterPayload *) tsn->sendSlot.msg->data;
		tp->thruster_bits = payload->thruster_bits;
		net_send_message(tsn->network, &tsn->sendSlot);
		tsn->last_send = clock_time_us();
	}
}

void thruster_message_sent(SendSlot *sendSlot)
{
	sendSlot->sending = FALSE;
}

