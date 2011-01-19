#include "momentary_display.h"

void momentary_display_init(MomentaryDisplay *md, Time display_period, uint8_t board_num)
{
	md->act.func = &_momentary_display_update;
	md->display_period = display_period;
	md->board_num = board_num;

	md->recvSlot.func = _momentary_display_recv;
	md->recvSlot.port = MUSIC_DISPLAY_PORT;
	md->recvSlot.payload_capacity = sizeof(MomentaryDisplayMessage);
	md->recvSlot.msg_occupied = FALSE;
	md->recvSlot.msg = (Message*) md->messagespace;
	md->recvSlot.user_data = md;
		

	board_buffer_init(&md->bbuf);
	md->is_visible = false;
	md->last_display = clock_time_us() - display_period;

	schedule_us(1, &md->act);
}

void _momentary_display_update(Activation *act)
{
	MomentaryDisplay *md = (MomentaryDisplay *) act;
	Time delta = (clock_time_us() - md->last_display);
	r_bool want_visible = (delta>0 && delta < md->display_period);
		// yeah, time will wrap, occasionally causing a spurious display.
		// meh.
	if (want_visible && !md->is_visible)
	{
		board_buffer_push(&md->bbuf, md->board_num);
	}
	else if (!want_visible && md->is_visible)
	{
		board_buffer_pop(&md->bbuf);
	}
	md->is_visible = want_visible;

	schedule_us(100000, &md->act);
}

void _momentary_display_recv(RecvSlot *recvSlot, uint8_t payload_len)
{
	MomentaryDisplay *md = (MomentaryDisplay *) recvSlot->user_data;
	assert(payload_len == sizeof(MomentaryDisplayMessage));
	MomentaryDisplayMessage *mdm = (MomentaryDisplayMessage *) &recvSlot->msg->data;
	board_buffer_ TODO LEFT OFF HERE drawing screen
	recvSlot->msg_occupied = FALSE;
}
