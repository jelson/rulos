#include "rocket.h"
#include "joystick.h"
#include "display_thrusters.h"

static void thrusters_update(ThrusterState_t *ts)
{
	char buf[4];

	// get state from joystick
	joystick_poll(&ts->joystick_state);

/*
	LOGF((logfp, "thrusters_update: got xpos %d, ypos %d\n", 
		  ts->joystick_state.x_pos,
		  ts->joystick_state.y_pos));
*/

	// display X and Y positions
	int_to_string2(buf, 3, 2, ts->joystick_state.x_pos);
	ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, buf);
	int_to_string2(buf, 3, 2, ts->joystick_state.y_pos);
	ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, buf);

	// display X and Y thresholds
	if (ts->joystick_state.state & JOYSTICK_LEFT)
		ts->bbuf.buffer[1] |= SSB_DECIMAL;
	else if (ts->joystick_state.state & JOYSTICK_RIGHT)
		ts->bbuf.buffer[3] |= SSB_DECIMAL;
	else
		ts->bbuf.buffer[2] |= SSB_DECIMAL;

	if (ts->joystick_state.state & JOYSTICK_DOWN)
		ts->bbuf.buffer[5] |= SSB_DECIMAL;
	else if (ts->joystick_state.state & JOYSTICK_UP)
		ts->bbuf.buffer[7] |= SSB_DECIMAL;
	else
		ts->bbuf.buffer[6] |= SSB_DECIMAL;

	// fire thrusters, baby!!
	//
	// current logic: only one thruster is firing at a time.
	// UP         = thruster A.
	// DOWN&LEFT  = thruster B.
	// DOWN&RIGHT = thruster C.
	ts->bbuf.buffer[0] = 0xff;
	if (ts->joystick_state.state & JOYSTICK_UP)
		ts->bbuf.buffer[0] = ~(SSB_SEG_a);

	if (ts->joystick_state.state & JOYSTICK_DOWN &&
		ts->joystick_state.state & JOYSTICK_LEFT)
		ts->bbuf.buffer[0] = ~(SSB_SEG_b);

	if (ts->joystick_state.state & JOYSTICK_DOWN &&
		ts->joystick_state.state & JOYSTICK_RIGHT)
		ts->bbuf.buffer[0] = ~(SSB_SEG_c);

	if (later_than(clock_time_us(), ts->last_send + THRUSTER_REPORT_INTERVAL)
		&& !ts->sendSlot.sending)
	{
		ThrusterPayload *tp = (ThrusterPayload *) ts->sendSlot.msg->data;
		tp->thruster_bits = (~(ts->bbuf.buffer[0]>>4)) & 0x07;
		net_send_message(ts->network, &ts->sendSlot);
		ts->last_send = clock_time_us();
	}

	// commit output -- both digits and physical thrusters!
	board_buffer_draw(&ts->bbuf);

	schedule_us(10000, (Activation *) ts);
}

void thruster_message_sent(SendSlot *sendSlot)
{
	sendSlot->sending = FALSE;
}

void thrusters_init(ThrusterState_t *ts,
					uint8_t board,
					uint8_t x_adc_channel,
					uint8_t y_adc_channel,
					Network *network)
{
	// initialize the joystick
	ts->joystick_state.x_adc_channel = x_adc_channel;
	ts->joystick_state.y_adc_channel = y_adc_channel;
	joystick_init(&ts->joystick_state);

	ts->func = (ActivationFunc) thrusters_update;
	board_buffer_init(&ts->bbuf);
	board_buffer_push(&ts->bbuf, board);

	ts->network = network;
	ts->sendSlot.func = thruster_message_sent;
	ts->sendSlot.msg = (Message*) ts->thruster_message_storage;
	ts->sendSlot.msg->dest_port = THRUSTER_PORT;
	ts->sendSlot.msg->message_size = sizeof(ThrusterPayload);
	ts->sendSlot.sending = FALSE;
	ts->last_send = clock_time_us();

	// Digits 0 and 4 are the HPAMs.  Set latches low (by setting
	// segments high, since we have common anodes on the LEDs)
	ts->bbuf.buffer[0] = 0xff;
	ts->bbuf.buffer[4] = 0xff;

	schedule_us(1, (Activation *) ts);
}
