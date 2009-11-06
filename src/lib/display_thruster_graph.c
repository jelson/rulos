#include "rocket.h"
#include "display_thruster_graph.h"


void dtg_update(DThrusterGraph *act);
void dtg_draw_skinny_bars(BoardBuffer *bbuf, int v, SSBitmap bm);
void dtg_recv_func(RecvSlot *recvSlot);

void dtg_init(DThrusterGraph *dtg, uint8_t board, Network *network)
{
	dtg->func = (ActivationFunc) dtg_update;
	board_buffer_init(&dtg->bbuf);
	board_buffer_push(&dtg->bbuf, board);

	dtg->network = network;
	dtg->recvSlot.func = dtg_recv_func;
	dtg->recvSlot.port = THRUSTER_PORT;
	dtg->recvSlot.payload_capacity = sizeof(ThrusterPayload);
	dtg->recvSlot.msg_occupied = FALSE;
	dtg->recvSlot.msg = (Message*) dtg->thruster_message_storage;
	dtg->self = dtg;

	net_bind_receiver(dtg->network, &dtg->recvSlot);

	schedule_us(1, (Activation*) dtg);
}

void dtg_update(DThrusterGraph *dtg)
{
	schedule_us(Exp2Time(16), (Activation*) dtg);

	int d;
	for (d=0; d<NUM_DIGITS; d++) { dtg->bbuf.buffer[d] = 0; }
	dtg_draw_skinny_bars(&dtg->bbuf, (dtg->thruster_bits & 0x01) ? 16 : 0, 0b1000000);
	dtg_draw_skinny_bars(&dtg->bbuf, (dtg->thruster_bits & 0x02) ? 16 : 0, 0b0000001);
	dtg_draw_skinny_bars(&dtg->bbuf, (dtg->thruster_bits & 0x04) ? 16 : 0, 0b0001000);
	
	board_buffer_draw(&dtg->bbuf);
}

void dtg_draw_skinny_bars(BoardBuffer *bbuf, int v, SSBitmap bm)
{
	int d;
	for (d=0; d<NUM_DIGITS; d++)
	{
		if (d*2  <v) { bbuf->buffer[d] |= bm; }
	}
}

void dtg_recv_func(RecvSlot *recvSlot)
{
	DThrusterGraph *dtg = *((DThrusterGraph **)(recvSlot+1));
	ThrusterPayload *tp = (ThrusterPayload*) dtg->recvSlot.msg->data;
	dtg->thruster_bits = tp->thruster_bits;
	dtg->recvSlot.msg_occupied = FALSE;
	LOGF((logfp, "dtg_recv_func got bits %1x!\n", dtg->thruster_bits & 0x7));
}
