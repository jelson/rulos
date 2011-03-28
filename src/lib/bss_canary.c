#include "bss_canary.h"

#if SIM
void bss_canary_init(uint8_t *bss_end)
{
}
#else

#define BSS_CANARY_MAGIC 0xca

void _bss_canary_update(void *data);

void bss_canary_init(uint8_t *bss_end)
{
	bss_end[0] = BSS_CANARY_MAGIC;

	schedule_us(250000, _bss_canary_update, bss_end);
}

void _bss_canary_update(void *data)
{
	assert(bss_end[0] == BSS_CANARY_MAGIC);
	schedule_us(250000, _bss_canary_update, bss_end);
}
#endif
