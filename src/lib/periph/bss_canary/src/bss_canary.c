#include "bss_canary.h"

#include "rulos.h"

#if SIM
void bss_canary_init()
{
}
#else

#define BSS_CANARY_MAGIC 0xca

void _bss_canary_update(void *data);

uint8_t bss_end[0];

void bss_canary_init()
{
	bss_end[0] = BSS_CANARY_MAGIC;

	schedule_us(250000, _bss_canary_update, NULL);
}

void _bss_canary_update(void *data)
{
	assert(bss_end[0] == BSS_CANARY_MAGIC);
	schedule_us(250000, _bss_canary_update, NULL);
}
#endif
