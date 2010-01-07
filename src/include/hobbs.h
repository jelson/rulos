#ifndef _HOBBS_H
#define _HOBBS_H

#include "rocket.h"
#include "hpam.h"
#include "idle.h"

typedef struct {
	UIEventHandlerFunc func;
	HPAM *hpam;
} Hobbs;

void init_hobbs(Hobbs *hobbs, HPAM *hpam, IdleAct *idle);

#endif // _HOBBS_H
