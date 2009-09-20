#include "util.h"
#include <stdio.h>

#if SIM
FILE *logfp = NULL;

void init_util()
{
	logfp = fopen("log", "w");
}
#else //!SIM
#endif
