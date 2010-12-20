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

#ifndef _BOOSTER_H
#define _BOOSTER_H

#include "rocket.h"
#include "hpam.h"
#include "audio_client.h"
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
