#include "random.h"
// prng from http://inglorion.net/software/deadbeef_rand/

static uint32_t deadbeef_seed;
static uint32_t deadbeef_beef = 0xdeadbeef;

uint32_t deadbeef_rand()
{
	deadbeef_seed = (deadbeef_seed << 7) ^ ((deadbeef_seed >> 25) + deadbeef_beef);
	deadbeef_beef = (deadbeef_beef << 7) ^ ((deadbeef_beef >> 25) + 0xdeadbeef);
	return deadbeef_seed;
}

void deadbeef_srand(uint32_t x)
{
	deadbeef_seed = x;
	deadbeef_beef = 0xdeadbeef;
} 
