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

#include "chip/avr/periph/pov/pov.h"
#include "core/clock.h"
#include "core/cpumon.h"
#include "core/hal.h"
#include "mirror.h"
#include "periph/input_controller/input_controller.h"
#include "periph/laserfont/laserfont.h"
#include "periph/rocket/rocket.h"

int main() {
  heap_init();
  util_init();
  rulos_hal_init();
  clock_init(300);

  InputControllerAct ia;
  input_controller_init(&ia, NULL);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  MirrorHandler mirror;
  mirror_init(&mirror);

  PovHandler pov;
  pov_init(&pov, &mirror, 0, 1);

  extern LaserFont font_uni_05_x;  // yuk. Need to generate a .h, I guess.
  laserfont_draw_string(&font_uni_05_x, pov.bitmap, POV_BITMAP_LENGTH,
                        "01234 Hello, lasery world!");

#if SIM
  {
    FILE *fp = fopen("bitmap.txt", "w");
    int i, bit;
    for (i = 0; i < POV_BITMAP_LENGTH; i++) {
      SSBitmap p = pov.bitmap[i];
      for (bit = 0; bit < 8; bit++) {
        fprintf(fp, ((p >> bit) & 1) ? "#" : " ");
      }
      fprintf(fp, "\n");
    }
    fclose(fp);
  }
#endif  // SIM

  board_buffer_module_init();

  cpumon_main_loop();

  return 0;
}
