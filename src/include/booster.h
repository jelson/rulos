#ifndef _BOOSTER_H
#define _BOOSTER_H

#include "rocket.h"
#include "hpam.h"
#include "audio_server.h"
#include "screenblanker.h"

typedef struct {
	r_bool status;
	HPAM *hpam;
	AudioClient *audioClient;
	ScreenBlanker *screenblanker;
} Booster;

void booster_init(Booster *booster, HPAM *hpam, AudioClient *audioClient, ScreenBlanker *screenblanker);
void booster_set(Booster *booster, r_bool status);

#endif // _BOOSTER_H
