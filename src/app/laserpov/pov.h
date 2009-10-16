#ifndef _pov_h
#define _pov_h

#include "rocket.h"
#include "mirror.h"

#define POV_BITMAP_LENGTH (256)

typedef struct
{
	ActivationFunc func;
	MirrorHandler *mirror;
	uint8_t laser_board;
	uint8_t laser_digit;
	SSBitmap bitmap[POV_BITMAP_LENGTH];
} PovHandler;

void pov_init(PovHandler *pov, MirrorHandler *mirror, uint8_t laser_board, uint8_t laser_digit);

#endif // _pov_h
