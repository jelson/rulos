#include "rocket.h"
#include "display_boardid.h"

static void update(BoardActivation_t *ba)
{
	SSBitmap b_bitmap = ascii_to_bitmap('b');
	SSBitmap d_bitmap = ascii_to_bitmap('d');

	uint8_t board, digit;

	for (board = 0; board < NUM_BOARDS; board++) {
		if (ba->stage == 0)
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = b_bitmap;
		else if (ba->stage == 1)
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = ascii_to_bitmap(board + '0');
		else if (ba->stage == 2)
			for (digit = 0; digit < NUM_DIGITS; digit++)
				ba->b[board].buffer[digit] = d_bitmap;
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

void boardid_init(BoardActivation_t *ba)
{
	int board;

	ba->f = (ActivationFunc) update;
	ba->stage = 0;

	for (board = 0; board < NUM_BOARDS; board++) {
		board_buffer_init(&ba->b[board]);
		board_buffer_push(&ba->b[board], board);
	}

	schedule_us(500000, (Activation *) ba);
}

