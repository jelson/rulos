#ifndef _IDLE_H
#define _IDLE_H

#include "rocket.h"
#include "thruster_protocol.h"

#define MAX_IDLE_HANDLERS 4
//#define IDLE_PERIOD (((Time)1000000)*60*5)	// five minutes.
#define IDLE_PERIOD (((Time)1000000)*3)			// 3 seconds (test)
	// NB can't make it more than half the clock rollover time, or
	// later_than will screw up.  I think that's about 10 min.

struct s_idle_act;

#if 0
typedef struct {
	ThrusterUpdateFunc func;
	struct s_idle_act *idle;
} IdleThrusterListener;
#endif

typedef struct s_idle_act {
	ActivationFunc func;
	UIEventHandler *handlers[MAX_IDLE_HANDLERS];
	uint8_t num_handlers;
	r_bool nowactive;
	Time last_touch;
#if 0
	IdleThrusterListener thrusterListener;
#endif
} IdleAct;

void init_idle(IdleAct *idle);
void idle_add_handler(IdleAct *idle, UIEventHandler *handler);
void idle_touch(IdleAct *idle);

#endif // _IDLE_H
