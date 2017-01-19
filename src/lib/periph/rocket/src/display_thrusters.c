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

	if (ts->joystick_state.state & JOYSTICK_DISCONNECTED) {
	  ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, " NO");
	  ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, "JOy");
	} else {
		if (ts->joystick_state.state & JOYSTICK_TRIGGER) {
			ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, "PSH");
			ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, "PSH");
		} else {
			int_to_string2(buf, 3, 2, ts->joystick_state.x_pos);
			ascii_to_bitmap_str(&ts->bbuf.buffer[1], 3, buf);
			int_to_string2(buf, 3, 2, ts->joystick_state.y_pos);
			ascii_to_bitmap_str(&ts->bbuf.buffer[5], 3, buf);
		}

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
	}

	// fire thrusters, baby!!
	//
	// current logic: only one thruster is firing at a time.
	// UP         = thruster A.
	// DOWN&LEFT  = thruster B.
	// DOWN&RIGHT = thruster C.
	hpam_set_port(ts->hpam, hpam_thruster_rear,
		ts->joystick_state.state & JOYSTICK_UP);
	hpam_set_port(ts->hpam, hpam_thruster_frontright,
		ts->joystick_state.state & JOYSTICK_DOWN &&
			ts->joystick_state.state & JOYSTICK_LEFT);
	hpam_set_port(ts->hpam, hpam_thruster_frontleft,
		ts->joystick_state.state & JOYSTICK_DOWN &&
			ts->joystick_state.state & JOYSTICK_RIGHT);

	if ((ts->joystick_state.state & (JOYSTICK_UP | JOYSTICK_LEFT | JOYSTICK_RIGHT | JOYSTICK_TRIGGER)) != 0)
	{
		//LOGF((logfp, "idle touch due to nonzero joystick: %d\n", ts->joystick_state.state));
		if (ts->idle != NULL)
			idle_touch(ts->idle);
	}

	// draw digits
	board_buffer_draw(&ts->bbuf);

	schedule_us(10000, (ActivationFuncPtr) thrusters_update, ts);
}

void thrusters_init(ThrusterState_t *ts,
					uint8_t board,
					uint8_t x_adc_channel,
					uint8_t y_adc_channel,
					HPAM *hpam,
					IdleAct *idle)
{
	// initialize the joystick
	ts->joystick_state.x_adc_channel = x_adc_channel;
	ts->joystick_state.y_adc_channel = y_adc_channel;
	joystick_init(&ts->joystick_state);

	board_buffer_init(&ts->bbuf DBG_BBUF_LABEL("thrusters"));
	// mask off HPAM digits, so HPAM 'display' shows through
	board_buffer_set_alpha(&ts->bbuf, 0x77);
	board_buffer_push(&ts->bbuf, board);

	ts->hpam = hpam;
	ts->idle = idle;

	schedule_us(1, (ActivationFuncPtr) thrusters_update, ts);
}