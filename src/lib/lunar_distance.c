#include "lunar_distance.h"

void lunar_distance_update(LunarDistance *ld);

#define LD_CRUISE_SPEED (-237)
#define LUNAR_DISTANCE 237674

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0, uint8_t speed_b0)
{
	ld->func = (ActivationFunc) lunar_distance_update;
	drift_anim_init(&ld->da, 0, LUNAR_DISTANCE, 0, LUNAR_DISTANCE, 2376);
	board_buffer_init(&ld->dist_board DBG_BBUF_LABEL("dist"));
	board_buffer_push(&ld->dist_board, dist_b0);
	board_buffer_init(&ld->speed_board DBG_BBUF_LABEL("speed"));
	board_buffer_push(&ld->speed_board, speed_b0);
	da_set_velocity(&ld->da, 0);
	schedule_us(1, (Activation*) ld);
}

void lunar_distance_reset(LunarDistance *ld)
{
	da_set_value(&ld->da, LUNAR_DISTANCE);
	da_set_velocity(&ld->da, 0);
}

void lunar_distance_set_velocity_256ths(LunarDistance *ld, uint16_t frac)
{
	da_set_velocity(&ld->da, (((int32_t) LD_CRUISE_SPEED) * frac) >> 8);
}

void lunar_distance_update(LunarDistance *ld)
{
	schedule_us(100000, (Activation*) ld);

	if (da_read(&ld->da)==0)
	{
		// oh. we got there. Stop things to avoid clock rollover effects.
		da_set_value(&ld->da, 0);
		// also, velocity should be zero. :v)
		da_set_velocity(&ld->da, 0);
	}

	char buf[NUM_DIGITS+1];
	//LOGF((logfp, "lunar dist %d\n", da_read(&ld->da)));
	int_to_string2(buf, NUM_DIGITS, 0, da_read(&ld->da));
	ascii_to_bitmap_str(ld->dist_board.buffer, NUM_DIGITS, buf);
	ld->dist_board.buffer[NUM_DIGITS-4] |= SSB_DECIMAL;
	board_buffer_draw(&ld->dist_board);

	int32_t fps = -ld->da.velocity * ((uint32_t)5280 * 1024 / 1000);

	// add .03% noise to speed display
	if (fps>100)
	{
		int32_t noise = (fps/10000) * (deadbeef_rand() & 0x3);
		fps += noise;
	}

	int_to_string2(buf, NUM_DIGITS, 0, fps);
	//strcpy(&buf[6], "fs");
	ascii_to_bitmap_str(ld->speed_board.buffer, NUM_DIGITS, buf);
	if (fps>999) {
		ld->speed_board.buffer[4] |= SSB_DECIMAL;
	}
	if (fps>999999) {
		ld->speed_board.buffer[1] |= SSB_DECIMAL;
	}
	board_buffer_draw(&ld->speed_board);
}
