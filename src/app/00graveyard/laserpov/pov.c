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

#include <string.h>

#include "chip/avr/periph/pov/pov.h"

void pov_update(PovHandler *pov);

void pov_init(PovHandler *pov, MirrorHandler *mirror, uint8_t laser_board, uint8_t laser_digit)
{
	pov->func = (ActivationFunc) pov_update;
	pov->mirror = mirror;
	pov->laser_board = laser_board;
	pov->laser_digit = laser_digit;

	int i;
	for (i=0; i<POV_BITMAP_LENGTH; i++)
	{
		pov->bitmap[i] = i & 0x0ff;
	}

	schedule_us(1, (Activation*) pov);
}

void pov_update(PovHandler *pov)
{
	// update pov display at clock frequency
	schedule_us(1, (Activation*) pov);

	if (pov->mirror->period==0)
	{
		// can't divide by that; wait.
		return;
	}

	Time now = clock_time_us();
	// TODO can we afford a divide on every update?
	uint32_t frac = ((now - pov->mirror->last_interrupt)*POV_BITMAP_LENGTH) / pov->mirror->period;

	if (frac<0 || frac>POV_BITMAP_LENGTH)
	{
		// out of bitmap range; ignore.
		return;
	}

	SSBitmap bm = pov->bitmap[frac];
	program_cell(pov->laser_board, pov->laser_digit, bm);
}
