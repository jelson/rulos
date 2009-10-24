#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"


/************************************************************************************/
/************************************************************************************/

typedef struct {
	ActivationFunc f;
	BoardBuffer bbuf;
	uint16_t *adc0;
	uint16_t *adc1;
} ADCAct_t;


static void update(ADCAct_t *a)
{
	char buf[9];

	int_to_string2(buf,   4, 0, *a->adc0);
	int_to_string2(buf+4, 4, 0, *a->adc1);
	buf[8] = '\0';

	ascii_to_bitmap_str(a->bbuf.buffer, 8, buf);
	board_buffer_draw(&a->bbuf);
	schedule_us(10000, (Activation *) a);
}


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);
	board_buffer_module_init();

	ADCAct_t a;
	a.f = (ActivationFunc) update;
	board_buffer_init(&a.bbuf);
	board_buffer_push(&a.bbuf, 7);
	a.adc0 = hal_get_adc(2);
	a.adc1 = hal_get_adc(3);

	schedule_us(1, (Activation *) &a);
	cpumon_main_loop();
	return 0;
}

