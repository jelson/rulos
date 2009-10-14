#include <string.h>
#include "pov.h"

void pov_update(PovHandler *pov);

void pov_init(PovHandler *pov, MirrorHandler *mirror, uint8_t laser_board, uint8_t laser_digit)
{
	pov->func = (ActivationFunc) pov_update;
	pov->mirror = mirror;
	pov->laser_board = laser_board;
	pov->laser_digit = laser_digit;

	int i;
	for (i=0; i<BITMAP_LENGTH; i++)
	{
		pov->bitmap[i] = i & 0x0ff;
	}

	schedule(1, (Activation*) pov);
}

void pov_update(PovHandler *pov)
{
	schedule(1, (Activation*) pov);

	Time now = u_clock_time();
	// TODO can we afford a divide on every update?
	uint32_t frac = ((now - pov->mirror->last_interrupt)*BITMAP_LENGTH) / pov->mirror->period;

	if (frac<0 || frac>BITMAP_LENGTH)
	{
		// out of bitmap range; ignore.
		return;
	}

	SSBitmap bm = pov->bitmap[frac];
	program_cell(pov->laser_board, pov->laser_digit, bm);
}
