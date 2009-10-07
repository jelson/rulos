#include "rocket.h"

#if SIM

FILE *logfp = NULL;

void util_init()
{
	logfp = fopen("log", "w");
}
#else //!SIM
void util_init()
{
}
#endif

int32_t bound(int32_t v, int32_t l, int32_t h)
{
	if (v<l) { return l; }
	if (v>h) { return h; }
	return v;
}

uint32_t isqrt(uint32_t v)
{
	// http://www.embedded.com/98/9802fe2.htm, listing 2
	uint32_t square = 1;
	uint32_t delta = 3;
	while (square < v)
	{
		square += delta;
		delta += 2;
	}
	return (delta/2-1);
}

int int_div_with_correct_truncation(int a, int b)
{
	if (b<0) { a=-a; b=-b; }	// b is positive
	if (a>0)
	{
		return a/b;
	}
	else
	{
		int tmp = (-a)/b;
		if (tmp*b < -a)
		{
			// got truncated; round *up*
			tmp += 1;
		}
		return -tmp;
	}
}


