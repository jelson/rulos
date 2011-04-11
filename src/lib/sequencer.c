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
#include "sequencer.h"
#include "sound.h"
#include "screenblanker.h"

#define STUB(s)	{}
#define LAUNCH_COUNTDOWN_TIME (20*1000000+500000)
	// TODO half-second fudge factor may go away in real hardware; may be
	// due to playback error in simulator.
#define	L_LAUNCH_DURATION_MS	10001
#define LAUNCH_CLOCK_PERIOD 10000

void launch_configure_state(Launch *launch, LaunchState newState);
void launch_clock_update(Launch *launch);
void launch_configure_lunar_distance(Launch *launch);
UIEventDisposition launch_uie_handler(Launch *launch, UIEvent event);

void launch_init(Launch *launch, Screen4 *s4, Booster *booster, AudioClient *audioClient, struct s_screen_blanker *screenblanker)
{
	launch->func = (UIEventHandlerFunc) launch_uie_handler;

	launch->booster = booster;
	launch->audioClient = audioClient;

	launch->state = launch_state_init;	// force configuration

	launch->s4 = s4;
	dscrlmsg_init(&launch->dscrlmsg, s4->board0, "", 120);
	board_buffer_init(&launch->code_bbuf DBG_BBUF_LABEL("code"));
	board_buffer_init(&launch->textentry_bbuf DBG_BBUF_LABEL("launch"));
	RowRegion rowregion = {&launch->textentry_bbuf, 0, NUM_DIGITS};
	numeric_input_init(
		&launch->textentry, rowregion, (UIEventHandler*) launch, NULL, "");
	raster_big_digit_init(&launch->bigDigit, s4);

	launch->main_rtc = NULL;
	launch->lunar_distance = NULL;
	launch->launch_code = 0;

	launch->screenblanker = screenblanker;

	launch_configure_state(launch, launch_state_hidden);
	schedule_us(1, (ActivationFuncPtr) launch_clock_update, launch);
}

void launch_configure_state(Launch *launch, LaunchState newState)
{
	if (newState==launch->state)
	{
		return;
	}

	// teardown
	booster_set(launch->booster, FALSE);

	if (board_buffer_is_stacked(&launch->code_bbuf))
	{
		board_buffer_pop(&launch->code_bbuf);
	}
	if (board_buffer_is_stacked(&launch->textentry_bbuf))
	{
		// be sure cursor is removed before we try to pop textentry's bbuf
		launch->textentry.handler.func((UIEventHandler*) &launch->textentry.handler, uie_escape);
		board_buffer_pop(&launch->textentry_bbuf);
	}
	if (board_buffer_is_stacked(&launch->dscrlmsg.bbuf))
	{
		board_buffer_pop(&launch->dscrlmsg.bbuf);
	}
	launch->bigDigit.focused = FALSE;
	r_bool need_s4_hide = TRUE;

	launch->state = newState;

	// setup
	if (launch->state == launch_state_hidden)
	{
		booster_set(launch->booster, FALSE);
		ac_skip_to_clip(launch->audioClient, AUDIO_STREAM_BURST_EFFECTS, sound_silence, sound_silence);

		launch->launch_code = 0;
	}
	else
	{
		need_s4_hide = FALSE;
		raster_clear_buffers(&launch->s4->rrect);
	}

	if (launch->state == launch_state_enter_code)
	{

		if (launch->launch_code==0)
		{
			// create a new launch code
			deadbeef_srand(clock_time_us());
			uint8_t digit;
			launch->launch_code = 0;
			// insist on nonzero first digit, to keep text entry nonconfusing
			// (numeric_input treats input as integers, so it hides leading zeros).
			launch->launch_code = (deadbeef_rand() % 9)+1;
			for (digit=1; digit<4; digit++)
			{
				launch->launch_code = launch->launch_code*10 + (deadbeef_rand() % 10);
			}
		}

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
#define LAUNCH_ENTER_CODE_MSG "Initiate launch sequence. Enter code xxxx.  "
			assert(strlen(LAUNCH_ENTER_CODE_MSG) < sizeof(launch->launch_code_str));
			strcpy(launch->launch_code_str, LAUNCH_ENTER_CODE_MSG);
			char code_str[8];
			code_str[0] = 'c';
			code_str[1] = 'o';
			code_str[2] = 'd';
			code_str[3] = 'e';
			code_str[4] = launch->launch_code_str[37] = '0' + ((launch->launch_code / 1000) % 10);
			code_str[5] = launch->launch_code_str[38] = '0' + ((launch->launch_code /  100) % 10);
			code_str[6] = launch->launch_code_str[39] = '0' + ((launch->launch_code /   10) % 10);
			code_str[7] = launch->launch_code_str[40] = '0' + ((launch->launch_code /    1) % 10);
			dscrlmsg_set_msg(&launch->dscrlmsg, launch->launch_code_str);

			ascii_to_bitmap_str(launch->code_bbuf.buffer, NUM_DIGITS, code_str);
			board_buffer_draw(&launch->code_bbuf);
		}
		else
		{
			dscrlmsg_set_msg(&launch->dscrlmsg,
				"Invalid code.  ");
			launch->nextEventTimeout = clock_time_us()+3000000;
		}
		board_buffer_push(&launch->dscrlmsg.bbuf, launch->s4->board0+0);
	}

	if (launch->state == launch_state_enter_code)
	{
		board_buffer_push(&launch->code_bbuf, launch->s4->board0+1);
		board_buffer_push(&launch->textentry_bbuf, launch->s4->board0+2);
		launch->textentry.handler.func(
			(UIEventHandler*) &launch->textentry.handler, uie_focus);
	}

	if (launch->state == launch_state_countdown)
	{
		launch->bigDigit.focused = TRUE;
		launch->bigDigit.startTime = clock_time_us()+LAUNCH_COUNTDOWN_TIME;
		launch->nextEventTimeout = clock_time_us()+LAUNCH_COUNTDOWN_TIME;
		if (launch->main_rtc)
		{
			drtc_set_base_time(launch->main_rtc, clock_time_us()+LAUNCH_COUNTDOWN_TIME);
		}
		ac_skip_to_clip(launch->audioClient, AUDIO_STREAM_BURST_EFFECTS, sound_apollo_11_countdown, sound_silence);
	}

	if (launch->state == launch_state_launching)
	{
		booster_set(launch->booster, TRUE);
		ascii_to_bitmap_str(launch->s4->bbuf[1].buffer, 8, " BLAST");
		ascii_to_bitmap_str(launch->s4->bbuf[2].buffer, 8, "  OFF");
		launch->nextEventTimeout = clock_time_us()+10000000;
	}

	if (launch->state == launch_state_complete)
	{
		booster_set(launch->booster, FALSE);
		ascii_to_bitmap_str(launch->s4->bbuf[1].buffer, 8, " boost");
		ascii_to_bitmap_str(launch->s4->bbuf[2].buffer, 8, "complete");
	}

	if (need_s4_hide)
	{
		s4_hide(launch->s4);
	}
	else
	{
		s4_show(launch->s4);
		s4_draw(launch->s4);
	}
}

void launch_clock_update(Launch *launch)
{
	schedule_us(LAUNCH_CLOCK_PERIOD, (ActivationFuncPtr) launch_clock_update, launch);

	// animation elements -- evaluate every tick
	launch_configure_lunar_distance(launch);

	// timeout elements -- evaluate once timer expires.
	if (!later_than(clock_time_us(), launch->nextEventTimeout))
	{
		return;
	}

	switch (launch->state)
	{
		case launch_state_init:
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
						if (launch->textentry.cur_value.mantissa == launch->launch_code
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
				switch (event)
				{
					// cheater button advances clock.
					// But it doesn't help with the audio clip, which will
					// run out of sync.
					case uie_right:
						launch->bigDigit.startTime -= 1000000;
						launch->nextEventTimeout -= 1000000;
						if (launch->main_rtc)
						{
							drtc_set_base_time(launch->main_rtc,
								launch->main_rtc->base_time - 1000000);
						}
						break;
				}
			case launch_state_init:
			case launch_state_launching:
			case launch_state_complete:
				break;
		}
	}
	return disposition;
}
