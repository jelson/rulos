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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"


/************************************************************************************/
/************************************************************************************/

typedef struct {
	ActivationFunc f;
	int stage;
	BoardBuffer b[8];
	SSBitmap b_bitmap, d_bitmap;
} BoardIDAct_t;


static void update(BoardIDAct_t *ba)
{
	uint8_t board, digit;

	for (board = 0; board < NUM_BOARDS; board++) {
		switch (ba->stage) {
		case 0:
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ba->b_bitmap;
			break;

		case 1:
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ascii_to_bitmap(board + '0');
			break;

		case 2:
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ba->d_bitmap;
			break;

		case 3:
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ascii_to_bitmap(digit + '0');
			break;

		default:
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = (1 << (11-ba->stage));
			
		}

		board_buffer_draw(&ba->b[board]);
	}
	if (++(ba->stage) == 12)
		ba->stage = 0;

	schedule_us(1000000, (Activation *)ba);
	return;
}


int main()
{
	util_init();
	hal_init(bc_audioboard);
	init_clock(100000, TIMER1);
	board_buffer_module_init();

	BoardIDAct_t ba;
	ba.f = (ActivationFunc) update;
	ba.stage = 0;
	ba.b_bitmap = ascii_to_bitmap('b');
	ba.d_bitmap = ascii_to_bitmap('d');

	uint8_t board;
	for (board = 0; board < NUM_BOARDS; board++) {
		board_buffer_init(&ba.b[board]);
		board_buffer_push(&ba.b[board], board);
	}

	schedule_now((Activation *) &ba);

	//	KeyTestActivation_t kta;
	//	display_keytest_init(&kta, 7);

	cpumon_main_loop();
	return 0;
}

