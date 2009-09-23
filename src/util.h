#ifndef _util_h
#define _util_h

#define FALSE 0
#define TRUE 1

#ifdef SIM

#include <stdio.h>
#include <assert.h>

#define say(x)	{ fprintf(stderr, "say: %s\n", x); }
extern FILE *logfp;
#define LOGF(x)	{ fprintf x; fflush(logfp); }

#else	//!SIM

#define assert(x)	{ if (!x) { /* TODO display assert on a rocket panel! :v) */ } }
#define LOGF(x)	{}
#endif //SIM

void init_util();

#endif // _util_h
