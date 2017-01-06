/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "rulos.h"
#include "hardware.h"

#define FREQ_USEC 50000
#define TEST_PIN GPIO_B5
#define TOTAL 20
#define TEST_HAL

#if TEST_SCHEDULER
void test_func(void *data)
{
	gpio_set(TEST_PIN);
	schedule_us(FREQ_USEC, a);
	gpio_clr(TEST_PIN);
}
#endif

void hal_test_func(void *data)
{
	gpio_set(TEST_PIN);
	gpio_clr(TEST_PIN);
}

int main()
{
	hal_init();
	gpio_make_output(TEST_PIN);
	gpio_clr(TEST_PIN);

#ifdef TEST_HAL
	hal_start_clock_us(FREQ_USEC, hal_test_func, NULL, TIMER1);
	while(1) { } ;

#endif

#ifdef TEST_SCHEDULER
	init_clock(FREQ_USEC, TIMER1);

	a.func = test_func;

	int i;
	for (i = 0; i < TOTAL-1; i++)
		schedule_us(1000000+i, &a);

	schedule_now(&a);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
#endif
}
