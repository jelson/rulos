#include "util.h"

#if SIM

#include <stdio.h>

FILE *logfp = NULL;

void init_util()
{
	logfp = fopen("log", "w");
}
#else //!SIM
void init_util()
{
}
#endif

int32_t bound(int32_t v, int32_t l, int32_t h)
{
	if (v<l) { return l; }
	if (v>h) { return h; }
	return v;
}
