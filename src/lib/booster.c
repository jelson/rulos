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
	hpam_set_port(booster->hpam, hpam_booster, FALSE);
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
		ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_CONTINUOUS_EFFECTS, sound_booster_start, sound_booster_running);
		screenblanker_setmode(booster->screenblanker, sb_flicker);
	}
	else
	{
		booster->status = FALSE;
		hpam_set_port(booster->hpam, hpam_booster, FALSE);
		ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_CONTINUOUS_EFFECTS, sound_booster_flameout_cutoff, sound_space_background);
		screenblanker_setmode(booster->screenblanker, sb_inactive);
	}
}

