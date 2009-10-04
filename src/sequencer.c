#include "rocket.h"
#include "sequencer.h"

#define STUB(s)	{}
#define LAUNCH_CODE	4671
#define STRINGY(s)	#s

void launch_set_state(Launch *launch, LaunchState state, uint32_t time_ms);
void launch_config_panels(Launch *launch, LaunchPanelConfig new0, LaunchPanelConfig new1);
void launch_enter_state_ready(Launch *launch);
void launch_enter_state_enter_code(Launch *launch);
void launch_enter_state_countdown(Launch *launch);
void launch_enter_state_launching(Launch *launch);
void launch_enter_state_wrong_code(Launch *launch);
void launch_enter_state_complete(Launch *launch);
UIEventDisposition launch_sequencer_state_machine(UIEventHandler *handler, UIEvent event);

void launch_init(Launch *launch, uint8_t board0)
{
	launch->func = launch_sequencer_state_machine;
	launch->board0 = board0;
	launch->p0_config = lpc_blank;
	launch->p1_config = lpc_blank;

	dscrlmsg_init(&launch->p0_scroller, board0, " ", 100);
	board_buffer_pop(&launch->p0_scroller.bbuf);	// hold p0_config invariant

	blinker_init(&launch->p0_blinker, 250);

	drtc_init(&launch->p1_timer, board0);
	board_buffer_pop(&launch->p1_timer.bbuf);	// hold p1_config invariant

	board_buffer_init(&launch->textentry_bbuf);
	RowRegion rowregion = { &launch->textentry_bbuf, 0, 8 };
	numeric_input_init(&launch->p1_textentry, rowregion, (UIEventHandler*) launch, NULL, NULL);

	launch_enter_state_ready(launch);
}

void launch_set_state(Launch *launch, LaunchState state, uint32_t time_ms)
{
	launch->state = state;
	launch->state_start_time = clock_time();
	launch->timer_expired = FALSE;
	launch->timer_deadline = launch->state_start_time + time_ms;
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

	switch (launch->p1_config)
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
	launch_config_panels(launch, lpc_p0scroller, lpc_blank);
	dscrlmsg_set_msg(&launch->p0_scroller, "Ready.  ");
	launch_set_state(launch, launch_state_ready, 0);
}

void launch_enter_state_enter_code(Launch *launch)
{
	launch_config_panels(launch, lpc_p0scroller, lpc_p1textentry);
	dscrlmsg_set_msg(&launch->p0_scroller, "Initiate launch sequence. Enter code " STRINGY(LAUNCH_CODE) ".  ");
	STUB(direct_focus(&launch->p1_textentry));
	launch_set_state(launch, launch_state_enter_code, 0);
}

void launch_enter_state_countdown(Launch *launch)
{
	launch_config_panels(launch, lpc_p0scroller, lpc_p1timer);
	dscrlmsg_set_msg(&launch->p0_scroller, "Launch sequence initiated. Countdown.  ");
	launch_set_state(launch, launch_state_countdown, 10001);
}

const char *launch_message[] = {"", " LAUNCH ", NULL};
void launch_enter_state_launching(Launch *launch)
{
	launch_config_panels(launch, lpc_p0scroller, lpc_p1timer);
	blinker_set_msg(&launch->p0_blinker, launch_message);
	STUB(valve_control(VALVE_BOOSTER, 1));
	STUB(sound_start(SOUND_LAUNCH_NOISE));
	launch_set_state(launch, launch_state_launching, 10001);
}

const char *wrong_message[] = {"INVALID", "  CODE  ", NULL};
void launch_enter_state_wrong_code(Launch *launch)
{
	STUB(sound_start(SOUND_KLAXON));
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
	switch (launch->state)
	{
		case launch_state_ready:
			switch (event)
			{
				case uie_select:
					launch_enter_state_enter_code(launch);
					break;
			}
			break;
		case launch_state_enter_code:
			switch (event)
			{
				case launch_evt_notify_code:
					if (launch->p1_textentry.cur_value.mantissa == LAUNCH_CODE)
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
	return disposition;
}

