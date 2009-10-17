#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"


/*******************************************************************************/
/*******************************************************************************/


int main()
{
	hal_init();

#if 0
	extern uint32_t f_cpu;
	extern uint8_t *sevseg_digits;
	program_cell(0, 0, sevseg_digits[f_cpu/1000000]);
#endif

#if 0
	while (1)
	{
		int seg, dig, onoff;

		for (onoff = 0; onoff < 2; onoff++) {
			for (seg = 0; seg < 8; seg++) {
				for (dig = 0; dig < 8; dig++) {
						hal_program_segment(0, dig, seg, onoff);
				}
				hal_delay_ms(200);
			}
		}
	}
#endif

	while (1) {
		int bit, dig;

		for (bit = 0; bit < 8; bit++) {
			for (dig = 0; dig < 8; dig++) {
				SSBitmap b = (1 << bit);
				if (dig == 2 || dig == 4)
					hal_upside_down_led(&b);
				program_cell(0, dig, b);
			}
			hal_delay_ms(200);
		}
	}

	return 0;
}

