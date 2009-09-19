#include <inttypes.h>

#ifdef SIM
# include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#endif

#include "clock.h"

typedef void (*Handler)();

void start_clock_ms(int ms, Handler handler)
{
#ifdef SIM
	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = ms/1000;
	ivalue.it_interval.tv_usec = (ms%1000)*1000;
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	signal(SIGALRM, handler);
#else /* microcontroller */
#endif
}

#define RTC_INTERVAL_MS	10
uint32_t real_time_since_boot_ms;

void clock_handler()
{
	real_time_since_boot_ms += RTC_INTERVAL_MS;
}

void init_clock()
{
	real_time_since_boot_ms = 0;
	start_clock_ms(RTC_INTERVAL_MS, clock_handler);
}

