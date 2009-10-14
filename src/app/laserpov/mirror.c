#include <stdio.h>
#include "mirror.h"
#include "hardware.h"

void mirror_handler();
static MirrorHandler *theMirror = NULL;

void mirror_init(MirrorHandler *mirror)
{
	mirror->last_interrupt = 0;
	mirror->period = 0;
	assert(theMirror!=NULL);
	theMirror = mirror;
	sensor_interrupt_register_handler(mirror_handler);
}

void mirror_handler()
{
	UTime now = u_clock_time();
	theMirror->period = now - theMirror->last_interrupt;
	theMirror->last_interrupt = now;
}
