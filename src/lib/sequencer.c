#include "rocket.h"
#include "sequencer.h"
#include "sound.h"

#define STUB(s)	{}
#define LAUNCH_CODE		4671
#define LAUNCH_CODE_S	"4671"
#define	L_LAUNCH_DURATION_MS	10001
#define LAUNCH_CLOCK_PERIOD 10000

void launch_configure_state(Launch *launch, LaunchState newState);
void launch_clock_update(LaunchClockAct *launchClockAct);
void launch_configure_lunar_distance(Launch *launch);
UIEventDisposition launch_uie_handler(Launch *launch, UIEvent event);

void launch_init(Launch *launch, uint8_t board0)
{
	launch->func = (UIEventHandlerFunc) launch_uie_handler;
	launch->clock_act.func = (ActivationFunc) launch_clock_update;
	launch->clock_act.launch = launch;

	launch->state = -1;	// force configuration

	init_screen4(&launch->s4, board0);
	dscrlmsg_init(&launch->dscrlmsg, board0, "", 120);
	board_buffer_init(&launch->textentry_bbuf);
	RowRegion rowregion = {&launch->textentry_bbuf, 0, NUM_DIGITS};
	numeric_input_init(
		&launch->textentry, rowregion, (UIEventHandler*) launch, NULL, "");
	raster_big_digit_init(&launch->bigDigit, board0);

	launch->main_rtc = NULL;
	launch->lunar_distance = NULL;

	launch_configure_state(launch, launch_state_hidden);
	schedule_us(1, (Activation*) &launch->clock_act);
}

void launch_configure_state(Launch *launch, LaunchState newState)
{
	if (newState==launch->state)
	{
		return;
	}

	// teardown
	if (board_buffer_is_stacked(&launch->textentry_bbuf))
	{
		board_buffer_pop(&launch->textentry_bbuf);
	}
	if (board_buffer_is_stacked(&launch->dscrlmsg.bbuf))
	{
		board_buffer_pop(&launch->dscrlmsg.bbuf);
	}
	s4_hide(&launch->bigDigit.s4);
	s4_hide(&launch->s4);

	launch->state = newState;

	// setup
	if (launch->state != launch_state_hidden)
	{
		s4_show(&launch->s4);
		raster_clear_buffers(&launch->s4.rrect);
	}

	if (launch->state == launch_state_enter_code)
	{
		if (launch->lunar_distance != NULL)
		{
			lunar_distance_reset(launch->lunar_distance);
		}
	}
	if (launch->state == launch_state_enter_code
		|| launch->state == launch_state_wrong_code)
	{
		if (launch->state == launch_state_enter_code)
		{
			dscrlmsg_set_msg(&launch->dscrlmsg,
				"Initiate launch sequence. Enter code " LAUNCH_CODE_S ".  ");
		}
		else
		{
			dscrlmsg_set_msg(&launch->dscrlmsg,
				"Invalid code.  ");
			launch->nextEventTimeout = clock_time_us()+3000000;
		}
		board_buffer_push(&launch->dscrlmsg.bbuf, launch->s4.board0+0);
	}

	if (launch->state == launch_state_enter_code)
	{
		board_buffer_push(&launch->textentry_bbuf, launch->s4.board0+2);
		launch->textentry.handler.func(
			(UIEventHandler*) &launch->textentry.handler, uie_focus);
	}

	if (launch->state == launch_state_countdown)
	{
		s4_show(&launch->bigDigit.s4);
		launch->bigDigit.startTime = clock_time_us()+10000000;
		launch->nextEventTimeout = clock_time_us()+10000000;
		if (launch->main_rtc)
		{
			drtc_set_base_time(launch->main_rtc, clock_time_us()+10000000);
		}
	}

	if (launch->state == launch_state_launching)
	{
		ascii_to_bitmap_str(launch->s4.bbuf[1].buffer, 8, " BLAST");
		ascii_to_bitmap_str(launch->s4.bbuf[2].buffer, 8, "  OFF");
		launch->nextEventTimeout = clock_time_us()+10000000;
	}

	if (launch->state == launch_state_complete)
	{
		ascii_to_bitmap_str(launch->s4.bbuf[1].buffer, 8, " boost");
		ascii_to_bitmap_str(launch->s4.bbuf[2].buffer, 8, "complete");
	}

	s4_draw(&launch->s4);
}

void launch_clock_update(LaunchClockAct *launchClockAct)
{
	Launch *launch = launchClockAct->launch;
	schedule_us(LAUNCH_CLOCK_PERIOD, (Activation*) &launch->clock_act);

	// animation elements -- evaluate every tick
	launch_configure_lunar_distance(launch);

	// timeout elements -- evaluate once timer expires.
	if (!later_than(clock_time_us(), launch->nextEventTimeout))
	{
		return;
	}

	switch (launch->state)
	{
		case launch_state_hidden:
		case launch_state_enter_code:
		case launch_state_complete:
			// no timeout here.
			break;
		case launch_state_wrong_code:
			launch_configure_state(launch, launch_state_enter_code);
			break;
		case launch_state_countdown:
			launch_configure_state(launch, launch_state_launching);
			break;
		case launch_state_launching:
			launch_configure_state(launch, launch_state_complete);
			break;
	}
}

void launch_configure_lunar_distance(Launch *launch)
{
	if (launch->lunar_distance != NULL
		&& launch->main_rtc != NULL)
	{
		if (launch->state == launch_state_launching)
		{
			uint16_t ship_velocity_frac = 0;
			Time launch_base = drtc_get_base_time(launch->main_rtc);
			Time elapsed_time = clock_time_us() - launch_base;
			if (elapsed_time/1000 > L_LAUNCH_DURATION_MS)
			{
				ship_velocity_frac = 256;
			}
			else if (elapsed_time >= 0)
			{
				ship_velocity_frac = (elapsed_time/1000 * 256) / L_LAUNCH_DURATION_MS;
			}
			//LOGF((logfp, "elapsed time %d frac %d\n", elapsed_time, ship_velocity_frac));
			lunar_distance_set_velocity_256ths(launch->lunar_distance, ship_velocity_frac);
		}
	}
}

UIEventDisposition launch_uie_handler(Launch *launch, UIEvent event)
{
	UIEventDisposition disposition = uied_accepted;
	if (event == uie_escape)
	{
		launch_configure_state(launch, launch_state_hidden);
		disposition = uied_blur;
	}
	else
	{
		switch (launch->state)
		{
			case launch_state_hidden:
				switch (event)
				{
					case uie_focus:
					case uie_select:
						launch_configure_state(launch, launch_state_enter_code);
						break;
				}
				break;
			case launch_state_enter_code:
				switch (event)
				{
					case evt_notify:
						if (launch->textentry.cur_value.mantissa == LAUNCH_CODE
							&& launch->textentry.cur_value.neg_exponent == 0)
						{
							launch_configure_state(launch, launch_state_countdown);
						}
						else
						{
							launch_configure_state(launch, launch_state_wrong_code);
						}
						break;
					case uie_escape:
						break;
					default:
						launch->textentry.handler.func(
							(UIEventHandler*) &launch->textentry.handler, event);
						break;
				}
				break;
			case launch_state_wrong_code:
			case launch_state_countdown:
			case launch_state_launching:
			case launch_state_complete:
				break;
		}
	}
	return disposition;
}
