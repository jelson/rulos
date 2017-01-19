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

#ifndef _mirror_h
#define _mirror_h

#include "rocket.h"

typedef struct
{
	Time last_interrupt;
	Time period;
} MirrorHandler;

// NB this can only be called once per program, because it owns the
// hardware interrupt handler.
void mirror_init(MirrorHandler *mirror);

#endif // _mirror_h