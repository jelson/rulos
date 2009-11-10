#include "rocket.h"
#include "sequencer.h"
#include "sound.h"

#define STUB(s)	{}
#define LAUNCH_CODE		4671
#define LAUNCH_CODE_S	"4671"
#define	L_LAUNCH_DURATION_MS	10001

#if 0
void launch_set_state(Launch *launch, LaunchState state, Time time_ms);
void launch_config_panels(Launch *launch, LaunchPanelConfig new0, LaunchPanelConfig new1);
void launch_enter_state_ready(Launch *launch);
void launch_enter_state_enter_code(Launch *launch);
void launch_enter_state_countdown(Launch *launch);
void launch_enter_state_launching(Launch *launch);
void launch_enter_state_wrong_code(Launch *launch);
void launch_enter_state_complete(Launch *launch);
UIEventDisposition launch_uie_handler(Launch *launch, UIEvent event);
void launch_clock_handler(Activation *act);

void launch_init(Launch *launch, uint8_t board0, FocusManager *fa)
{
	launch->func = launch_uie_handler;
	launch->board0 = board0;

	int bbi;
	for (bbi=0; bbi<CONTROL_PANEL_HEIGHT; bbi++)
	{
		launch->btable[bbi] = &launch->bbuf[bbi];
		board_buffer_init(launch->btable[bbi]);
		board_buffer_push(launch->btable[bbi], board0+bbi);
	}
	launch->rrect.bbuf = launch->btable;
	launch->rrect.ylen = LAUNCH_HEIGHT;
	launch->rrect.x = 0;
	launch->rrect.xlen = 8;
	
	board_buffer_init(launch->textentry_bbuf);
	RowRegion rowregion = { &launch->textentry_bbuf, 0, 8 };
	numeric_input_init(&launch->textentry, rowregion, (UIEventHandler*) launch, NULL, NULL);
	
	launch->clock_act.func = launch_clock_handler;
	launch->clock_act.launch = launch;

	launch->main_rtc = NULL;
	launch->lunar_distance = NULL;

	schedule_us(1, (Activation*) &launch->clock_act);
}

void launch_timer_update()
{
	if (state==enter_code)
	{
		display textentry_bbuf
}

void launch_config_panels(Launch *launch)
{
}

	dscrlmsg_set_msg(&launch->p0_scroller, "Initiate launch sequence. Enter code " LAUNCH_CODE_S ".  ");
	// STUB(direct_focus(&launch->p1_textentry));
	// Rather than trying to route focus, we just pass clicks through.
	// But we need to tell widget that it got focused.
	launch->p1_textentry.handler.func(
		(UIEventHandler*) &launch->p1_textentry.handler, uie_focus);
	launch_set_state(launch, launch_state_enter_code, 0);
}

void launch_enter_state_countdown(Launch *launch)
{
	drtc_set_base_time(&launch->p1_timer, clock_time_us()+10000000);
	if (launch->main_rtc)
	{
		drtc_set_base_time(launch->main_rtc, clock_time_us()+10000000);
	}
	launch_config_panels(launch, lpc_p0scroller, lpc_p1timer);
	dscrlmsg_set_msg(&launch->p0_scroller, "Launch sequence initiated. Countdown.  ");
	launch_set_state(launch, launch_state_countdown, 10001);
}

const char *launch_message[] = {"", " LAUNCH ", NULL};
void launch_enter_state_launching(Launch *launch)
{
	launch_config_panels(launch, lpc_p0blinker, lpc_p1timer);
	blinker_set_msg(&launch->p0_blinker, launch_message);
	STUB(valve_control(VALVE_BOOSTER, 1));
	sound_start(sound_launch_noise, FALSE);
	launch_set_state(launch, launch_state_launching, L_LAUNCH_DURATION_MS);
}

const char *wrong_message[] = {"INVALID", "  CODE  ", NULL};
void launch_enter_state_wrong_code(Launch *launch)
{
	sound_start(sound_klaxon, FALSE);
	launch_config_panels(launch, lpc_p0blinker, lpc_blank);
	blinker_set_msg(&launch->p0_blinker, wrong_message);
	launch_set_state(launch, launch_state_wrong_code, 3000);
}

void launch_enter_state_complete(Launch *launch)
{
	launch_config_panels(launch, lpc_p0scroller, lpc_blank);
	dscrlmsg_set_msg(&launch->p0_scroller, "Launch complete. Orbit attained.  ");
	launch_set_state(launch, launch_state_complete, 0);
}

UIEventDisposition launch_uie_handler(Launch *launch, UIEvent event)
{
	UIEventDisposition disposition = uied_accepted;
	Launch *launch = (Launch*)handler;
	if (event == uie_escape)
	{
		launch_enter_state_ready(launch);
		disposition = uied_blur;
	}
	else
	{
		switch (launch->state)
		{
			case launch_state_ready:
				switch (event)
				{
					case uie_focus:
					case uie_select:
						launch_enter_state_enter_code(launch);
						break;
				}
				break;
			case launch_state_enter_code:
				switch (event)
				{
					case evt_notify:
						if (launch->p1_textentry.cur_value.mantissa == LAUNCH_CODE
							&& launch->p1_textentry.cur_value.neg_exponent == 0)
						{
							launch_enter_state_countdown(launch);
						}
						else
						{
							launch_enter_state_wrong_code(launch);
						}
						break;
					case uie_escape:
					case sequencer_timeout:
						break;
					default:
						launch->p1_textentry.handler.func(
							(UIEventHandler*) &launch->p1_textentry.handler, event);
						break;
				}
				break;
			case launch_state_wrong_code:
				switch (event)
				{
					case sequencer_timeout:
						launch_enter_state_enter_code(launch);
						break;
				}
				break;
			case launch_state_countdown:
				switch (event)
				{
					case sequencer_timeout:
						launch_enter_state_launching(launch);
						break;
				}
				break;
			case launch_state_launching:
				switch (event)
				{
					case sequencer_timeout:
						launch_enter_state_complete(launch);
						break;
				}
				break;
		}
	}
	return disposition;
}

void launch_clock_handler(Activation *act)
{
	LaunchClockAct *lca = (LaunchClockAct *) act;
	Launch *launch = lca->launch;
//	LOGF((logfp, "Funky, cold medina. launch %08x Act %08x func %08x\n",
//		(int)launch, (int) &launch->clock_act, (int)launch->clock_act.func));
	schedule_us(250000, (Activation*) &launch->clock_act);

	if (launch->lunar_distance)
	{
		if (launch->state == launch_state_launching)
		{
			uint16_t ship_velocity_frac = 0;
			Time launch_base = drtc_get_base_time(&launch->p1_timer);
			Time elapsed_time = clock_time_us() - launch_base;
			if (elapsed_time/1000 > L_LAUNCH_DURATION_MS)
			{
				ship_velocity_frac = 256;
			}
			else if (elapsed_time >= 0)
			{
				ship_velocity_frac = (elapsed_time/1000 * 256) / L_LAUNCH_DURATION_MS;
			}
			LOGF((logfp, "elapsed time %d frac %d\n", elapsed_time, ship_velocity_frac));
			lunar_distance_set_velocity_256ths(launch->lunar_distance, ship_velocity_frac);
		}
	}

	if (!launch->timer_expired && clock_time_us() >= launch->timer_deadline)
	{
		launch->timer_expired = TRUE;
		launch->func((UIEventHandler*) launch, sequencer_timeout);
	}
}
#endif
