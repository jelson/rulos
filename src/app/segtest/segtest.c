/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/rulos.h"
#include "periph/7seg_panel/7seg_panel.h"

/*******************************************************************************/
/*******************************************************************************/

int main() {
  hal_init();
  hal_init_rocketpanel();
#if 0
	int seg=0, dig=0;
	seg=0;
	dig=0;
#endif

#if 0
	extern uint32_t f_cpu;
	extern uint8_t *sevseg_digits;
	program_cell(0, 0, sevseg_digits[f_cpu/1000000]);
#endif

#if 0
	program_cell(0, 0, 0xff);
	while (1)
	{

		hal_program_segment(7, 0, 0, 0);
		hal_delay_ms(20);
		hal_program_segment(7, 0, 0, 1);
		hal_delay_ms(20);
	}
#endif

#if 0
	for (seg = 0; seg < 8; seg++) {
		for (dig = 0; dig < 8; dig++) {
			hal_program_segment(7, dig, seg, 1);
		}
	}
	while (1)
	{

		for (seg = 0; seg < 8; seg++) {
			for (dig = 0; dig < 8; dig++) {
				hal_program_segment(7, dig, seg, 0);
			}
			int i;
			for (i=0;i < 4; i++)			hal_delay_ms(250);
			for (dig = 0; dig < 8; dig++) {
				hal_program_segment(7, dig, seg, 1);
			}
		}
	}
#endif

#if 1
  while (1) {
    int seg, dig, onoff;

    for (onoff = 0; onoff < 2; onoff++) {
      for (seg = 0; seg < 8; seg++) {
        for (dig = 0; dig < 8; dig++) {
          hal_program_segment(0, dig, seg, onoff);
        }
        hal_delay_ms(1000);
      }
    }
  }
#endif

#if 0
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
#endif

  return 0;
}
