#include "rocket.h"
#include "hardware.h"

#define FREQ_USEC 100000
#define TEST_PIN GPIO_D7
#define TOTAL 20

Activation a;

void test_func(Activation *a)
{
	gpio_set(TEST_PIN);
	schedule_us(FREQ_USEC, a);
	gpio_clr(TEST_PIN);
}


int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(FREQ_USEC, TIMER1);

	gpio_make_output(TEST_PIN);
	gpio_clr(TEST_PIN);


	a.func = test_func;

	int i;
	for (i = 0; i < TOTAL-1; i++)
		schedule_us(1000000+i, &a);

	schedule_now(&a);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
}
