#ifndef _mirror_h
#define _mirror_h

#include "rocket.h"

typedef struct
{
	UTime last_interrupt;
	UTime period;
} MirrorHandler;

// NB this can only be called once per program, because it owns the
// hardware interrupt handler.
void mirror_init(MirrorHandler *mirror);

#endif // _mirror_h
