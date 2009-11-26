#ifndef __util_h__
#define __util_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#include <inttypes.h>

#define FALSE 0
#define TRUE 1
typedef uint8_t r_bool;
	// "r_bool" for ROCKET bool! ncurses and other libraries define
	// incompatible bools, so I want to avoid (rather than battle) collisions.

#ifdef SIM

#include <stdio.h>
#include <assert.h>

#define say(x)	{ fprintf(stderr, "say: %s\n", x); }
extern FILE *logfp;
#define LOGF(x)	{ fprintf x; fflush(logfp); }

#else	//!SIM

// jonh apologizes for evilly dup-declaring this here rather than
// rearranging includes to make sense.
extern void hardware_assert(uint16_t line);

#define assert(x)	{ if (!(x)) { hardware_assert(__LINE__); } }
#define LOGF(x)	{}
#endif //SIM

void util_init();
int32_t bound(int32_t v, int32_t l, int32_t h);
uint32_t isqrt(uint32_t v);

#define min(a,b)	(((a)<(b))?(a):(b))
#define max(a,b)	(((a)>(b))?(a):(b))

int int_div_with_correct_truncation(int a, int b);


#endif // _util_h
