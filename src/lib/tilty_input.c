#include "tilty_input.h"
#include "pov.h"	// for pov_paint

#define G_QUIESCENT	50
#define G_GRAVITY		100

#define TI_POV_SWITCH_TIME	2000000
#define TI_MIN_TOKEN_TIME   1000000
#define TI_MAX_TOKEN_TIME   2000000

#define TI_SCAN_PERIOD		  50000

void _tilty_input_issue_event(TiltyInputAct *tia, TiltyInputState evt)
{
	(tia->event_handler->func)(tia->event_handler, evt);
	tia->last_event_issued = evt;
}

static inline r_bool is_quiescent(int16_t value)
{
	return (value>-G_QUIESCENT && value<G_QUIESCENT);
}

static inline TiltyInputState accel_to_state(Vect3D *accel)
{
	r_bool qx = is_quiescent(accel->x);
	r_bool qy = is_quiescent(accel->y);
	r_bool qz = is_quiescent(accel->z);

	if (qx && qy && accel->z>G_GRAVITY)
	{
		return ti_neutral;
	}
	if (qx && qz && accel->y<G_GRAVITY)
	{
		return ti_up;
	}
	if (qx && qz && accel->y>G_GRAVITY)
	{
		return ti_down;
	}
	if (qy && qz && accel->x<G_GRAVITY)
	{
		return ti_left;
	}
	if (qy && qz && accel->x>G_GRAVITY)
	{
		return ti_right;
	}
	// some other orientation we don't care about.
	return ti_undef;
}

void tilty_input_update(TiltyInputAct *tia)
{
	schedule_us(TI_SCAN_PERIOD, (Activation *) tia);

	TiltyInputState newState = accel_to_state(tia->accelValue);

	Time time_now = clock_time_us();

	if (newState==ti_undef)
	{
		return;
		// yawn.
	}

	Time input_duration = time_now - tia->enterTime;

	char *dbg_node = "_none_";
	r_bool blink = FALSE;
	if (newState==ti_neutral)
	{
		dbg_node = "ti_neutral";
		if (tia->last_event_issued == ti_enter_pov)
		{
			dbg_node = "was_enter_pov";
			if (tia->currentState == ti_neutral)
			{
				// be sure we're counting time in neutral state
				dbg_node = "cs==ti_neutral";
				// we were in POV state. Must hang out here long enough
				// to clear that state before beginning a fresh input.
				if (input_duration > TI_POV_SWITCH_TIME)
				{
					dbg_node = "exit_pov";
					_tilty_input_issue_event(tia, ti_exit_pov);
				}
			}
		}
		else if (tia->currentState != ti_neutral)
		{
			dbg_node = "wasnt_enter_pov";
			// we were in input state
			if (input_duration > TI_MIN_TOKEN_TIME
				&& input_duration < TI_MAX_TOKEN_TIME)
			{
				dbg_node = "click";
				// hey, that's a "click"! Issue a keystroke for completed state.
				_tilty_input_issue_event(tia, tia->currentState);
				blink = TRUE;
			}
		}
	}
	else if (newState==ti_up && tia->currentState==ti_up)
	{
		dbg_node = "ti_up";
		if (tia->last_event_issued != ti_enter_pov
			&& input_duration > TI_POV_SWITCH_TIME)
		{
			dbg_node = "enter_pov";
			_tilty_input_issue_event(tia, ti_enter_pov);
		}
	}

	// provide LED feedback
	if (tia->last_event_issued != ti_enter_pov)
	{
		if (blink)
		{
			pov_paint(0x1f);
		}
		else if (newState!=ti_undef && newState!=ti_neutral)
		{
			if (input_duration<TI_MIN_TOKEN_TIME)
			{
				pov_paint(0x04);
			}
			else if (input_duration<TI_MAX_TOKEN_TIME)
			{
				pov_paint(0x15);
			}
			else
			{
				pov_paint(0x00);
			}
		}
		else
		{
			pov_paint(0x00);
		}
	}

#if 0
	{
		void remote_debug(char *msg);
		static char buf[80];
		snprintf(buf, sizeof(buf)-1, "tiu: cs %02x newState %02x node %s dur %ld\n", tia->currentState, newState, dbg_node, input_duration);
		remote_debug(buf);
	}
#endif

	if (newState!=tia->currentState)
	{
		tia->currentState = newState;
		tia->enterTime = time_now;
	}
}

void tilty_input_init(TiltyInputAct *tia, Vect3D *accelValue, UIEventHandler *event_handler)
{
	tia->tilty_input_func_ptr = (ActivationFunc) tilty_input_update;

	tia->accelValue = accelValue;
	tia->event_handler = event_handler;

	tia->currentState = ti_undef;
	tia->enterTime = clock_time_us();
	tia->last_event_issued = ti_neutral;

	schedule_us(TI_SCAN_PERIOD, (Activation *) tia);
}
