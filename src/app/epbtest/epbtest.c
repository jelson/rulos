#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"


#define DISPLAYBOARD 0
/****************************************************************************/

typedef struct {
	ActivationFunc func;
} TestAct;

void test_update(TestAct *act);

void test_init(TestAct *act)
{
	act->func = (ActivationFunc) test_update;
	schedule_us(1, (Activation*) act);
}

void test_update(TestAct *act)
{
	schedule_us(1, (Activation*) act);
	uint8_t digit, segment, board;
	for (board = 0; board < NUM_BOARDS; board++) {
		if (board==DISPLAYBOARD) continue;
		for (digit = 0; digit < NUM_DIGITS; digit++) {
			for (segment = 0; segment < 8; segment++) {
				uint8_t onoff = (((clock_time_us()>>18)+digit) & 7) == segment;
				hal_program_segment(board, digit, segment, onoff);
			}
		}
	}

	uint16_t delay = hal_read_adc(0);
#if !SIM
	extern uint16_t g_epb_delay_constant;
	g_epb_delay_constant = delay;
#endif
	delay += 1;
	char buf[9];
	memset(buf, '-', 8);
	int_to_string2(buf, 6, 0, delay);
	SSBitmap bm[8];
	ascii_to_bitmap_str(bm, 8, buf);
	program_board(DISPLAYBOARD, bm);
}
/****************************************************************************/


int main()
{
	util_init();
	hal_init(bc_rocket0);
	init_clock(1000, TIMER1);
	hal_init_adc(10000);
	hal_init_adc_channel(0);

	TestAct test;
	test_init(&test);

	cpumon_main_loop();
	return 0;
}

