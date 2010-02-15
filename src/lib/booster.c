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
		ac_skip_to_clip(booster->audioClient, AUDIO_STREAM_CONTINUOUS_EFFECTS, sound_booster_flameout, sound_silence);
		screenblanker_setmode(booster->screenblanker, sb_inactive);
	}
}

