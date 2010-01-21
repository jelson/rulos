#include "slow_boot.h"

void slowboot_update(SlowBoot *slowboot);

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker, AudioClient *audioClient)
{
	slowboot->func = (ActivationFunc) slowboot_update;
	slowboot->screenblanker = screenblanker;
	slowboot->audioClient = audioClient;


#if BORROW_SCREENBLANKER_BUFS
	screenblanker_setmode(slowboot->screenblanker, sb_borrowed);
	slowboot->buffer = screenblanker->buffer;

#else // BORROW_SCREENBLANKER_BUFS
	// Ahem. Memory's a little tight, see, so we're turning the
	// screenblanker on and borrowing *it's* board buffers, which
	// otherwise would cost us 7*13 = 91 bytes of .bss. Cough cough.

	screenblanker_setmode(slowboot->screenblanker, sb_black);

	int bi;
	for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
	{
		board_buffer_init(&slowboot->buffer[bi]);
		board_buffer_push(&slowboot->buffer[bi], bi);
	}
#endif // BORROW_SCREENBLANKER_BUFS

	slowboot->startTime = clock_time_us();

	schedule_us(1, (Activation*) slowboot);
}

#define SB_ANIM_INTERVAL 100000

void slowboot_update(SlowBoot *slowboot)
{
	int bi;

	Time elapsedTime = clock_time_us() - slowboot->startTime;

	int8_t step = elapsedTime / SB_ANIM_INTERVAL;

	//LOGF((logfp, "\n"));
	if (step<4)
	{
		goto exit;
	}
	step -= 4;
	if (step<28)
	{
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			int dist = (step + ((bi * 131) % SLOW_MAX_BUFFERS))/4-2;
			//LOGF((logfp, "step %2d bi %2d dist %2d\n", step, bi, dist));
			if (dist<0 || dist>4) { continue; }
			memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
			slowboot->buffer[bi].buffer[dist] = SSB_SEG_e | SSB_SEG_f | SSB_SEG_g;
			slowboot->buffer[bi].buffer[NUM_DIGITS-1-dist] = SSB_SEG_b | SSB_SEG_c | SSB_SEG_g;
			int di;
			for (di=0; di<dist; di++)
			{
				slowboot->buffer[bi].buffer[di] = 0xff;
				slowboot->buffer[bi].buffer[NUM_DIGITS-1-di] = 0xff;
			}
			board_buffer_draw(&slowboot->buffer[bi]);
		}
		goto exit;
	}
	step -= 28;
	if (step<2)
	{
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
		}
		goto exit;
	}
	step -= 2;
	if (step==0)
	{
		ac_skip_to_clip(slowboot->audioClient, sound_quindar_key_down, sound_silence);
	}
	step /= 4;
	if (step < 5)
	{
		for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
		{
			memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
		}
		switch (step)
		{
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
	step -= 5;
	if (step<3)
	{
		goto exit;
	}

	// all done. Tear down display, and never schedule me again.
#if !BORROW_SCREENBLANKER_BUFS
	for (bi=0; bi<SLOW_MAX_BUFFERS; bi++)
	{
		board_buffer_pop(&slowboot->buffer[bi]);
	}
#endif // BORROW_SCREENBLANKER_BUFS
	screenblanker_setmode(slowboot->screenblanker, sb_inactive);
	return;

exit:
	schedule_us(SB_ANIM_INTERVAL, (Activation*) slowboot);
}
