#include "lunar_distance.h"

void lunar_distance_update(LunarDistance *ld);

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0, uint8_t speed_b0)
{
	ld->func = (ActivationFunc) lunar_distance_update;
	drift_anim_init(&ld->da, 0, 237674, 0, 237674, 2376);
	board_buffer_init(&ld->dist_board);
	board_buffer_push(&ld->dist_board, dist_b0);
	board_buffer_init(&ld->speed_board);
	board_buffer_push(&ld->speed_board, speed_b0);
	ld->da.velocity = -237;
	schedule_us(1, (Activation*) ld);
}

void lunar_distance_update(LunarDistance *ld)
{
	schedule_us(100000, (Activation*) ld);

	char buf[NUM_DIGITS+1];
	LOGF((logfp, "lunar dist %d\n", da_read(&ld->da)));
	int_to_string2(buf, NUM_DIGITS, 0, da_read(&ld->da));
	ascii_to_bitmap_str(ld->dist_board.buffer, NUM_DIGITS, buf);
	ld->dist_board.buffer[NUM_DIGITS-4] |= SSB_DECIMAL;
	board_buffer_draw(&ld->dist_board);

	int fps = -ld->da.velocity * ((uint32_t)5280 * 1024 / 1000);
	int_to_string2(buf, NUM_DIGITS-3, 0, fps);
	strcpy(&buf[6], "fs");
	ascii_to_bitmap_str(ld->speed_board.buffer, NUM_DIGITS, buf);
	ld->speed_board.buffer[NUM_DIGITS-6] |= SSB_DECIMAL;
	board_buffer_draw(&ld->speed_board);
}
