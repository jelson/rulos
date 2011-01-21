#include "bss_canary.h"

#if SIM
void bss_canary_init(BSSCanary *bc)
{
}
#else

#define BSS_CANARY_MAGIC 0xca

void _bss_canary_update(Activation *act);

uint8_t bss_end[0];

void bss_canary_init(BSSCanary *bc)
{
	bc->act.func = _bss_canary_update;

	bss_end[0] = BSS_CANARY_MAGIC;

	schedule_us(250000, &bc->act);
}

void _bss_canary_update(Activation *act)
{
	assert(bss_end[0] == BSS_CANARY_MAGIC);
	schedule_us(250000, act);
}
#endif
