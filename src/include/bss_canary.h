#ifndef _BSS_CANARY_H
#define _BSS_CANARY_H

#include <rocket.h>

typedef struct {
	Activation act;
} BSSCanary;

void bss_canary_init(BSSCanary *bc);

#endif // _BSS_CANARY_H
