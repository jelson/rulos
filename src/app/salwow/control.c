#include "control.h"

#define OBSERVATION_DURATION_SAMPLES 5
#define US_PER_DEGREE ((uint32_t)(60000000/360))
	// That's one minute for a 360-degree turn.

typedef enum { RUDDER_RIGHT=0, RUDDER_LEFT=1, RUDDER_STRAIGHT=2 } RudderRequest;

void _control_start_observing(void *v_ctl);
void _control_gps_read(void* v_ctl);
void _control_observe(Control *ctl);
void _control_start_turning(Control *ctl);

void _control_set_rudder(Control *ctl, RudderRequest req)
{
	switch (req)
	{
	case RUDDER_STRAIGHT:
		rudder_set_angle(&ctl->rudder,    0);
		break;
	case RUDDER_RIGHT:
		rudder_set_angle(&ctl->rudder,  100);
		break;
	case RUDDER_LEFT:
		rudder_set_angle(&ctl->rudder, -100);
		break;
	}
}

void control_init(Control *ctl)
{
	ctl->state = Acquiring;
	rudder_init(&ctl->rudder);

	ctl->n_waypoints = 0;
}

void control_add_waypoint(Control *ctl, Vector *wpt)
{
	assert(ctl->n_waypoints < MAX_WAYPOINTS);
	v_copy(&ctl->waypoints[ctl->n_waypoints], wpt);
	ctl->n_waypoints += 1;
}

void _control_sequence_waypoint(Control *ctl, int index)
{
	assert(index>0);
	assert(index < ctl->n_waypoints);
	ctl->waypoint_index = index;
	navigation_activate_leg(&ctl->nav,
		&ctl->waypoints[ctl->waypoint_index-1], 
		&ctl->waypoints[ctl->waypoint_index]);
}

void control_start(Control *ctl)
{
	navigation_init(&ctl->nav, &ctl->waypoints[0]);

	_control_sequence_waypoint(ctl, 1);
	gpsinput_init(&ctl->gpsi, 1, _control_gps_read, ctl);
}

void _test_sentence_done(GPSInput* gpsi)
{
#ifndef SIM
	{
		char smsg[100];
		int latint = (int) gpsi->lat;
		uint32_t latfrac = (gpsi->lat - latint)*1000000;
		float poslon = -gpsi->lon;
		int lonint = (int) poslon;
		uint32_t lonfrac = (poslon - lonint)*1000000;
		sprintf(smsg, "GPS: %d.%06ld, %d.%06ld\n", latint,latfrac,lonint, lonfrac);
		uart_debug_log(smsg);
	}
#else
	fprintf(stderr, "Read %f,%f\n", (double) gpsi->lat, (double) gpsi->lon);
#endif // SIM
}

void _control_gps_read(void* v_ctl)
{
	Control *ctl = (Control*) v_ctl;

	_test_sentence_done(&ctl->gpsi);
	
	switch (ctl->state)
	{
	case Acquiring:
		schedule_now(_control_start_observing, ctl);
		return;
	case Observing:
		_control_observe(ctl);
	case Turning:
		// ignore GPS while turning
		return;
	case Completed:
		// ignore GPS when we're done!
		return;
	}
}

void _control_start_observing(void *v_ctl)
{
	uart_debug_log("_control_start_observing\n");
	Control *ctl = (Control*) v_ctl;
	_control_set_rudder(ctl, RUDDER_STRAIGHT);
	ctl->sample_num = 0;
	ctl->state = Observing;
	// next time GPS arrives, we'll accumulate data.
}

void _control_observe(Control *ctl)
{
	ctl->sample_num += 1;
	// We discard the first sample, as it reflects observations
	// from the prior turning period.
	if (ctl->sample_num == 2)
	{
		v_init(&ctl->p0, ctl->gpsi.lon, ctl->gpsi.lat);
	}
	else if (ctl->sample_num > OBSERVATION_DURATION_SAMPLES)
	{
		v_init(&ctl->p1, ctl->gpsi.lon, ctl->gpsi.lat);
		_control_start_turning(ctl);
	}
}

#define INTABS(x)	(((x)>0)?(x):(-x))

void _control_start_turning(Control *ctl)
{
	int turn_request;
	while (TRUE)
	{
		turn_request = navigation_compute(&ctl->nav, &ctl->p0, &ctl->p1);
		if (turn_request==ABEAM)
		{
			uart_debug_log("sequence waypoint\n");
			int next_index = ctl->waypoint_index + 1;
			if (next_index >= ctl->n_waypoints)
			{
				// OMFSM DONE!
				uart_debug_log("task complete\n");
				ctl->state = Completed;
				return;
			}
			_control_sequence_waypoint(ctl, next_index);
			continue;
		}
		// have a valid turn request
		break;
	}

	uart_debug_log(turn_request>0 ? "turn right\n" : "turn left\n");
	_control_set_rudder(ctl, turn_request>0 ? RUDDER_RIGHT : RUDDER_LEFT);
	Time turn_duration_us = INTABS(turn_request) * US_PER_DEGREE;
	ctl->state = Turning;
	schedule_us(turn_duration_us, _control_start_observing, ctl);
}
