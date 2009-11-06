#include "rocket.h"
#include "display_aer.h"


void daer_update(DAER *act);

void daer_init(DAER *daer, uint8_t board, Time impulse_frequency_us)
{
	daer->func = (ActivationFunc) daer_update;
	board_buffer_init(&daer->bbuf);
	board_buffer_push(&daer->bbuf, board);
	drift_anim_init(&daer->azimuth,   10, 320,   0, 360, 5);
	drift_anim_init(&daer->elevation, 10,   4,   0,  90, 5);
	drift_anim_init(&daer->roll,      10, -90, -90,  90, 5);
	daer->impulse_frequency_us = impulse_frequency_us;
	daer->last_impulse = 0;
	schedule_us(1, (Activation*) daer);
}

void daer_update(DAER *daer)
{
	schedule_us(Exp2Time(16), (Activation*) daer);

	Time t = clock_time_us();
	if (t - daer->last_impulse > daer->impulse_frequency_us)
	{
		da_random_impulse(&daer->azimuth);
		da_random_impulse(&daer->elevation);
		da_random_impulse(&daer->roll);
		daer->last_impulse = t;
	}

	char str[9];
	int_to_string2(str+0, 3, 3, da_read(&daer->azimuth));
	int_to_string2(str+3, 2, 2, da_read(&daer->elevation));
	int_to_string2(str+5, 3, 2, da_read(&daer->roll));

	ascii_to_bitmap_str(daer->bbuf.buffer, 8, str);
	board_buffer_draw(&daer->bbuf);
}
