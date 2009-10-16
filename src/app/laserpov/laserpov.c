#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "hal.h"
#include "cpumon.h"
#include "mirror.h"
#include "pov.h"
#include "input_controller.h"
#include "laserfont.h"

int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(300);

	InputControllerAct ia;
	input_controller_init(&ia, NULL);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	MirrorHandler mirror;
	mirror_init(&mirror);

	PovHandler pov;
	pov_init(&pov, &mirror, 0, 1);

	extern LaserFont font_uni_05_x;	// yuk. Need to generate a .h, I guess.
	laserfont_draw_string(&font_uni_05_x, pov.bitmap, POV_BITMAP_LENGTH, "01234 Hello, lasery world!");

#if SIM
	{
		FILE *fp = fopen("bitmap.txt", "w");
		int i, bit;
		for (i=0; i<POV_BITMAP_LENGTH; i++)
		{
			SSBitmap p = pov.bitmap[i];
			for (bit=0; bit<8; bit++)
			{
				fprintf(fp, ((p>>bit) & 1) ? "#" : " ");
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
	}
#endif // SIM

	board_buffer_module_init();

	cpumon_main_loop();

	return 0;
}

