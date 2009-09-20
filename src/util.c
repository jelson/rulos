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
