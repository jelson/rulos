#ifdef SIM

#include <stdio.h>
#include <assert.h>

#define say(x)	{ fprintf(stderr, "say: %s\n", x); }

#else	//!SIM
#define assert(x)	{ if (!x) { /* TODO display assert on a rocket panel! :v) */ } }
#endif //SIM
