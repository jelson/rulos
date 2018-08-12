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

#ifndef display_docking_h
#define display_docking_h

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/input_controller/focus.h"
#include "periph/rocket/drift_anim.h"
#include "periph/rocket/thruster_protocol.h"
#include "periph/rocket/screen4.h"
#include "periph/audio/audio_server.h"
#include "periph/rocket/booster.h"
#include "periph/joystick/joystick.h"

#define DOCK_HEIGHT 4
#define MAX_Y (DOCK_HEIGHT*6)
#define MAX_X (NUM_DIGITS*4)
#define CTR_X (MAX_X/2)
#define CTR_Y (MAX_Y/2)

struct s_ddockact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_ddockact *act;
} DDockHandler;

typedef struct s_ddockact {
	Screen4 *s4;
	DDockHandler handler;
	uint8_t focused;
	DriftAnim xd, yd, rd;
	uint32_t last_impulse_time;
	ThrusterPayload thrusterPayload;
	BoardBuffer auxboards[2];
	uint8_t auxboard_base;
	r_bool docking_complete;
	AudioClient *audioClient;
	Booster *booster;
	JoystickState_t *joystick;
} DDockAct;

void ddock_init(DDockAct *act, Screen4 *s4, uint8_t auxboard_base, AudioClient *audioClient, Booster *booster, JoystickState_t *joystick);
void ddock_reset(DDockAct *dd);
void ddock_thruster_update(DDockAct *act, ThrusterPayload *tp);

#endif // display_docking_h
