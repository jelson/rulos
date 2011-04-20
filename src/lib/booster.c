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

#include "booster.h"

void booster_init(Booster *booster, HPAM *hpam, AudioClient *audioClient, ScreenBlanker *screenblanker)
{
	booster->status = FALSE;
	booster->hpam = hpam;
	booster->audioClient = audioClient;
	booster->screenblanker = screenblanker;
	booster->bcontext = bcontext_liftoff;
	hpam_set_port(booster->hpam, hpam_booster, FALSE);
}

void booster_set_context(Booster *booster, BoosterContext bcontext)
{
	booster->bcontext = bcontext;
}

void booster_set(Booster *booster, r_bool status)
{
	if (status == booster->status)
	{
		return;
	}

	if (status)
	{
		booster->status = TRUE;
		hpam_set_port(booster->hpam, hpam_booster, TRUE);
		ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS, sound_booster_start, sound_booster_running);
		screenblanker_setmode(booster->screenblanker, sb_flicker);
	}
	else
	{
		booster->status = FALSE;
		hpam_set_port(booster->hpam, hpam_booster, FALSE);

		if (booster->bcontext == bcontext_liftoff)
		{
			ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS, sound_booster_flameout_cutoff, sound_silence);
			// goofball hack to deal with output buffer queue limit (1):
			// Get the booster cutoff audio message going first, then 100ms later,
			// while it's playing, issue the command that starts
			// the right background noises, so they'll appear when the
			// former runs out.
			schedule_us(100000, (ActivationFuncPtr) ambient_noise_boost_complete,
				&booster->audioClient->ambient_noise);
		}
		else if (booster->bcontext == bcontext_docking)
		{
			ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_BURST_EFFECTS, sound_booster_flameout, sound_silence);
		}
		else
		{
			assert(false);
		}

		screenblanker_setmode(booster->screenblanker, sb_inactive);
	}
}

