#include "hpam.h"

void hpam_update(HPAM *hpam);

#define REST_TIME_SECONDS	30
// Note that a clever pilot can work around rest times by firing for 29.9s,
// resting for 0.1s, firing again. They're here to protect valves from
// melting due to broken software.
#define REST_NONE	0
// Lights don't need rest

void _hpam_init_port(HPAM *hpam, HPAMIndex idx, Time max_time_sec, uint8_t board, uint8_t digit)
{
	hpam->hpam_ports[idx].status = FALSE;
	hpam->hpam_ports[idx].max_time = max_time_sec * 1000000;
	hpam->hpam_ports[idx].board = board;
	hpam->hpam_ports[idx].digit = digit;
	hpam->hpam_ports[idx].segment = idx;
	hpam->hpam_ports[idx].resting = FALSE;
}

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate **thrusterUpdates)
{
	hpam->func = (ActivationFunc) hpam_update;
	_hpam_init_port(hpam, hpam_empty_0, REST_NONE, board0, 0);
	_hpam_init_port(hpam, hpam_empty_1, REST_NONE, board0, 0);
	_hpam_init_port(hpam, hpam_empty_2, REST_NONE, board0, 0);
	_hpam_init_port(hpam, hpam_empty_3, REST_NONE, board0, 0);
	_hpam_init_port(hpam, hpam_thruster_frontleft, REST_TIME_SECONDS, board0, 0);
	_hpam_init_port(hpam, hpam_thruster_frontright, REST_TIME_SECONDS, board0, 0);
	_hpam_init_port(hpam, hpam_thruster_rear, REST_TIME_SECONDS, board0, 0);
	_hpam_init_port(hpam, hpam_booster, REST_TIME_SECONDS, board0, 0);
	hpam->thrusterUpdates = thrusterUpdates;

	board_buffer_init(&hpam->bbuf);
	board_buffer_set_alpha(&hpam->bbuf, 0x88);	// hard-code hpam digits 0,4
	board_buffer_push(&hpam->bbuf, board0);

	HPAMIndex idx;
	for (idx = 0; idx<hpam_end; idx++)
	{
		hpam_set_port(hpam, idx, TRUE);	// force a state change
		hpam_set_port(hpam, idx, FALSE);
	}

	schedule_us(1, (Activation*) hpam);
}

void hpam_update(HPAM *hpam)
{
	schedule_us(1000000, (Activation *) hpam);

	// check that no valve stays open more than max_time.
	HPAMIndex idx;
	for (idx = 0; idx<hpam_end; idx++)
	{
		HPAMPort *port = &hpam->hpam_ports[idx];
		if (port->status
			&& port->max_time > 0
			&& later_than(clock_time_us(), port->expire_time))
		{
			hpam_set_port(hpam, idx, FALSE);

			port->resting = TRUE;
			// 33% duty cycle
			port->rest_until = clock_time_us() + 2*port->max_time;
			LOGF((logfp, "RESTING HPAM idx %d\n", idx));
		}
		if (port->resting
			&& later_than(clock_time_us(), port->rest_until))
		{
			port->resting = FALSE;
			LOGF((logfp, "reactivating HPAM idx %d\n", idx));
		}
	}
}

void hpam_set_port(HPAM *hpam, HPAMIndex idx, r_bool status)
{
	assert(0<=idx && idx<hpam_end);

	HPAMPort *port = &hpam->hpam_ports[idx];
	if (port->status == status)
	{
		return;
	}
	if (port->resting)
	{
		// can't fiddle with it.
		return;
	}

	port->status = status;

	// drive the HPAM through the LED latch
	SSBitmap mask = (1<<port->segment);
	hpam->bbuf.buffer[port->digit] =
		(hpam->bbuf.buffer[port->digit]
			& ~mask)
		| (status ? 0 : mask);
//	LOGF((logfp, "idx %d status %d digit now %x\n",
//		idx, status, hpam->bbuf.buffer[port->digit]));

	board_buffer_draw(&hpam->bbuf);

	// update expire time (only matters for sets)
	port->expire_time = clock_time_us() + port->max_time;

	// Forward the notice to listeners
	ThrusterPayload payload;
	payload.thruster_bits = (~(hpam->bbuf.buffer[port->digit]));
	ThrusterUpdate **tu = hpam->thrusterUpdates;
	while (tu[0]!=NULL)
	{
		tu[0]->func(tu[0], &payload);
		tu++;
	}
}
