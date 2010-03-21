#include <inttypes.h>
#include <string.h>

#include "rocket.h"
#include "hardware.h"

#define REGISTER_SER  GPIO_D5  // serial data input
#define REGISTER_LAT  GPIO_D6  // assert register to parallel output lines
#define REGISTER_CLK  GPIO_D7  // shift in data to register

static void init_audio_pins()
{
	gpio_make_output(REGISTER_SER);
	gpio_make_output(REGISTER_LAT);
	gpio_make_output(REGISTER_CLK);
	gpio_clr(REGISTER_LAT);
	gpio_clr(REGISTER_CLK);
}

static inline void shift_out_8bits(uint8_t num)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		gpio_set_or_clr(REGISTER_SER, num & 0x1);
		gpio_set(REGISTER_CLK);
		gpio_clr(REGISTER_CLK);
		num >>= 1;
	}
}

static inline void latch_output()
{
	gpio_set(REGISTER_LAT);
	gpio_clr(REGISTER_LAT);
}


typedef struct {
	ActivationFunc f;
	int i;
} testAct_t;


void test_func(testAct_t *ta)
{
	schedule_us(1000, (Activation *) ta);

	// assert the previous int
	latch_output();

	// inc
	ta->i = (ta->i + 1) % 256;

	// shift the next int
	shift_out_8bits(ta->i);
	//shift_out_8bits(128);
}

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

	init_audio_pins();

	testAct_t ta;
	ta.f = (ActivationFunc) test_func;
	ta.i = 0;
	schedule_now((Activation *) &ta);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();

	return 0;
}
