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
} BoardIDAct_t;


static void update(BoardIDAct_t *ba)
{
	uint8_t board, digit;

	for (board = 0; board < NUM_BOARDS; board++) {
		if (ba->stage == 0) {
			SSBitmap b_bitmap = ascii_to_bitmap('b');
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = b_bitmap;
		}
		else if (ba->stage == 1)
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ascii_to_bitmap(board + '0');
		else if (ba->stage == 2) {
			SSBitmap d_bitmap = ascii_to_bitmap('d');
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = d_bitmap;
		}
		else if (ba->stage == 3)
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ascii_to_bitmap(digit + '0');
		board_buffer_draw(&ba->b[board]);
	}
	if (++(ba->stage) == 4)
		ba->stage = 0;

	schedule_us(1500000, (Activation *)ba);
	return;
}


int main()
{
	heap_init();
	util_init();
	hal_init();

#if 0
	int i;

	while (1){
		for (i = 0; i < 8; i++) {
			program_segment(0, 0, i, 0);
			_delay_ms(1000);
		}
		for (i = 0; i < 8; i++) {
			program_segment(0, 0, i, 1);
			_delay_ms(1000);
		}
	}
#endif

	clock_init(100000);
	board_buffer_module_init();

	BoardIDAct_t ba;
	ba.f = (ActivationFunc) update;
	ba.stage = 0;

	uint8_t board;
	for (board = 0; board < NUM_BOARDS; board++) {
		board_buffer_init(&ba.b[board]);
		board_buffer_push(&ba.b[board], board);
	}

	schedule_us(1, (Activation *) &ba);

	//	KeyTestActivation_t kta;
	//	display_keytest_init(&kta, 7);

	cpumon_main_loop();
	return 0;
}

