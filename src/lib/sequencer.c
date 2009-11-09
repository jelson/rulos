#include "rocket.h"
#include "sequencer.h"
#include "sound.h"

#define STUB(s)	{}
#define LAUNCH_CODE		4671
#define LAUNCH_CODE_S	"4671"

void launch_set_state(Launch *launch, LaunchState state, Time time_ms);
void launch_config_panels(Launch *launch, LaunchPanelConfig new0, LaunchPanelConfig new1);
void launch_enter_state_ready(Launch *launch);
void launch_enter_state_enter_code(Launch *launch);
void launch_enter_state_countdown(Launch *launch);
void launch_enter_state_launching(Launch *launch);
void launch_enter_state_wrong_code(Launch *launch);
void launch_enter_state_complete(Launch *launch);
UIEventDisposition launch_sequencer_state_machine(UIEventHandler *handler, UIEvent event);
void launch_clock_handler(Activation *act);

void launch_init(Launch *launch, uint8_t board0, FocusManager *fa)
{
	launch->func = launch_sequencer_state_machine;
	launch->board0 = board0;
	launch->p0_config = lpc_blank;
	launch->p1_config = lpc_blank;

	dscrlmsg_init(&launch->p0_scroller, board0, " ", 100);
	board_buffer_pop(&launch->p0_scroller.bbuf);	// hold p0_config invariant

	blinker_init(&launch->p0_blinker, 500);

	drtc_init(&launch->p1_timer, board0, 0);
	board_buffer_pop(&launch->p1_timer.bbuf);	// hold p1_config invariant

	board_buffer_init(&launch->textentry_bbuf);
	RowRegion rowregion = { &launch->textentry_bbuf, 0, 8 };
	numeric_input_init(&launch->p1_textentry, rowregion, (UIEventHandler*) launch, NULL, NULL);

	launch_enter_state_ready(launch);

	launch->bbufary[0] = &launch->p0_scroller.bbuf;
	launch->bbufary[1] = &launch->p1_timer.bbuf;
	if (fa!=NULL)
	{
		RectRegion rr = {launch->bbufary, 1, 0, 8};
		focus_register(fa, (UIEventHandler*) launch, rr, "launch");
	}

	launch->clock_act.func = launch_clock_handler;
	launch->clock_act.launch = launch;
	schedule_us(1, (Activation*) &launch->clock_act);
}

void launch_set_state(Launch *launch, LaunchState state, Time time_ms)
{
	launch->state = state;
	launch->state_start_time = clock_time_us();
	launch->timer_expired = FALSE;
	launch->timer_deadline = launch->state_start_time + time_ms*1000;
}

void launch_config_panels(Launch *launch, LaunchPanelConfig new0, LaunchPanelConfig new1)
{
	switch (launch->p0_config)
	{
		case lpc_p0scroller:
			board_buffer_pop(&launch->p0_scroller.bbuf);
			break;
		case lpc_p0blinker:
			board_buffer_pop(&launch->p0_blinker.bbuf);
			break;
		default:
			assert(launch->p1_config==lpc_blank);
	}
	launch->p0_config = lpc_blank;

	switch (launch->p1_config)
	{
		case lpc_p1timer:
			board_buffer_pop(&launch->p1_timer.bbuf);
			break;
		case lpc_p1textentry:
			// defocus textentry to make its cursor go away, so we can pop its bbuf
			launch->p1_textentry.handler.func(
				(UIEventHandler*) &launch->p1_textentry.handler, uie_escape);
			board_buffer_pop(&launch->textentry_bbuf);
			break;
		default:
			assert(launch->p1_config==lpc_blank);
	}
	launch->p1_config = lpc_blank;

	switch (new0)
	{
		case lpc_p0scroller:
			board_buffer_push(&launch->p0_scroller.bbuf, launch->board0+0);
			break;
		case lpc_p0blinker:
			board_buffer_push(&launch->p0_blinker.bbuf, launch->board0+0);
			break;
		default:
			assert(new0==lpc_blank);
	}
	launch->p0_config = new0;

	switch (new1)
	{
		case lpc_p1timer:
			board_buffer_push(&launch->p1_timer.bbuf, launch->board0+1);
			break;
		case lpc_p1textentry:
		{
			DecimalFloatingPoint dfp = {0,0};
			numeric_input_set_value(&launch->p1_textentry, dfp);
			board_buffer_push(&launch->textentry_bbuf, launch->board0+1);
			break;
		}
		default:
			assert(new1==lpc_blank);
	}
	launch->p1_config = new1;
}

void launch_enter_state_ready(Launch *launch)
{
	launch_config_panels(launch, lpc_blank, lpc_blank);
	dscrlmsg_set_msg(&launch->p0_scroller, "Ready.  ");
	launch_set_state(launch, launch_state_ready, 0);
}

void launch_enter_state_enter_code(Launch *launch)
{
	launch_config_panels(launch, lpc_p0scroller, lpc_p1textentry);
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
	launch_set_state(launch, launch_state_launching, 10001);
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

UIEventDisposition launch_sequencer_state_machine(UIEventHandler *handler, UIEvent event)
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
	if (!launch->timer_expired && clock_time_us() >= launch->timer_deadline)
	{
		launch->timer_expired = TRUE;
		launch->func((UIEventHandler*) launch, sequencer_timeout);
	}
}
