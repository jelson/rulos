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
#include "hardware.h"
#include "vect3d.h"


typedef enum {
	ti_neutral = 0,
	ti_undef = 0xa0,
	ti_left = 0xa1,
	ti_right = 0xa2,
	ti_up = 0xa3,
	ti_down = 0xa4,
	ti_enter_pov = 0xa5,
	ti_exit_pov = 0xa6,
} TiltyInputState;

typedef struct {
	ActivationFunc tilty_input_func_ptr;

	Vect3D *accelValue;
	UIEventHandler *event_handler;

	TiltyInputState currentState;
	Time enterTime;
	TiltyInputState last_event_issued;
} TiltyInputAct;

void _tilty_input_issue_event(TiltyInputAct *tia, TiltyInputState evt);
void tilty_input_update(TiltyInputAct *tia);
void tilty_input_init(TiltyInputAct *tia, Vect3D *accelValue, UIEventHandler *event_handler);

#endif // _TILTY_INPUT_H
