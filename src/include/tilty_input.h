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

#ifndef _TILTY_INPUT_H
#define _TILTY_INPUT_H

#include "rocket.h"

#ifndef SIM
#include "hardware.h"
#endif

#include "vect3d.h"


typedef enum {
	// internal states
	ti_neutral = 0,
	ti_undef = 0xa0,
	ti_left = 0xa1,
	ti_right = 0xa2,
	ti_up = 0xa3,
	ti_down = 0xa4,

	// messages sent out. (clumsy).
	ti_enter_pov = 0xa5,
	ti_exit_pov = 0xa6,

	// modifier flag
	ti_proposed = 0x100,
} TiltyInputState;

typedef enum {
	tia_led_click,
	tia_led_proposal,
	tia_led_black,
} TiltyLEDPattern;

typedef struct {
	Vect3D *accelValue;
	UIEventHandler *event_handler;

	TiltyInputState curState;
	Time proposed_time;

	TiltyLEDPattern led_pattern;
	Time led_anim_start;
} TiltyInputAct;

void _tilty_input_issue_event(TiltyInputAct *tia, TiltyInputState evt);
void tilty_input_update(TiltyInputAct *tia);
void tilty_input_init(TiltyInputAct *tia, Vect3D *accelValue, UIEventHandler *event_handler);

#endif // _TILTY_INPUT_H
