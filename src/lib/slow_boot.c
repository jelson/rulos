#include "slow_boot.h"

void slowboot_update(SlowBoot *slowboot);

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker, AudioClient *audioClient)
{
	slowboot->func = (ActivationFunc) slowboot_update;
	slowboot->screenblanker = screenblanker;
	slowboot->audioClient = audioClient;

	screenblanker_setmode(slowboot->screenblanker, sb_black);

	int bi;
	for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
	{
		board_buffer_init(&slowboot->buffer[bi]);
		board_buffer_push(&slowboot->buffer[bi], bi);
	}

	slowboot->startTime = clock_time_us();

	schedule_us(1, (Activation*) slowboot);
}

#define SB_ANIM_INTERVAL 500000

void slowboot_update(SlowBoot *slowboot)
{
	int bi;

	Time elapsedTime = clock_time_us() - slowboot->startTime;

	int8_t step = elapsedTime / SB_ANIM_INTERVAL;

	if (step<5)
	{
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
			slowboot->buffer[bi].buffer[step] = SSB_SEG_e | SSB_SEG_f | SSB_SEG_g;
			slowboot->buffer[bi].buffer[NUM_DIGITS-1-step] = SSB_SEG_b | SSB_SEG_c | SSB_SEG_g;
			int di;
			for (di=0; di<step; di++)
			{
				slowboot->buffer[bi].buffer[di] = 0xff;
				slowboot->buffer[bi].buffer[NUM_DIGITS-1-di] = 0xff;
			}
			board_buffer_draw(&slowboot->buffer[bi]);
		}
		goto exit;
	}
	step -= 6;
	if (step==0)
	{
		ac_skip_to_clip(slowboot->audioClient, sound_quindar_key_down, sound_silence);
	}
	if (step < 8)
	{
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
		}
		switch (step)
		{
			case 7:
			case 6:
			case 5:
			case 4:
				ascii_to_bitmap_str(slowboot->buffer[6].buffer, NUM_DIGITS, " Vehicle");
			case 3:
				ascii_to_bitmap_str(slowboot->buffer[5].buffer, NUM_DIGITS, "Altitude");
			case 2:
				ascii_to_bitmap_str(slowboot->buffer[4].buffer, NUM_DIGITS, "UltraLow");
			case 1:
				ascii_to_bitmap_str(slowboot->buffer[4].buffer, NUM_DIGITS, "Ultra");
			case 0:
				ascii_to_bitmap_str(slowboot->buffer[3].buffer, NUM_DIGITS, "Ravenna ");
		}
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			board_buffer_draw(&slowboot->buffer[bi]);
		}
		goto exit;
	}
	step -= 6;

	// all done. Tear down display, and never schedule me again.
	for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
	{
		board_buffer_pop(&slowboot->buffer[bi]);
	}
	screenblanker_setmode(slowboot->screenblanker, sb_inactive);
	return;

exit:
	schedule_us(SB_ANIM_INTERVAL, (Activation*) slowboot);
}
