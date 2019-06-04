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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/bss_canary/bss_canary.h"

#define UPDATE_RATE_US 40000

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif

#define DECODER_ENABLE GPIO_A3
#define DECODER_A0 GPIO_A0
#define DECODER_A1 GPIO_A1
#define DECODER_A2 GPIO_A2

#define LEDDRIVER_SDI GPIO_B5
#define LEDDRIVER_OE GPIO_A6
#define LEDDRIVER_CLK GPIO_B3
#define LEDDRIVER_LE GPIO_A7

typedef struct {
  int curr_value;
  int row;
} MatrixState_t;

MatrixState_t matrix_state;

void update_value(void *data) {
  schedule_us(UPDATE_RATE_US, (ActivationFuncPtr)update_value, data);

  MatrixState_t *ms = (MatrixState_t *)data;

  ms->curr_value++;
  if (ms->curr_value == 64) {
    ms->curr_value = 0;
    ms->row++;

    if (ms->row == 3) {
      ms->row = 0;
    }

    LOG("showing row %d", ms->row);
  }

  // Disable current display
  gpio_clr(DECODER_ENABLE);

  // Shift in value
  gpio_clr(LEDDRIVER_LE);
  for (int i = 0; i < 64; i++) {
    if (i == ms->curr_value) {
      gpio_set(LEDDRIVER_SDI);
    } else {
      gpio_clr(LEDDRIVER_SDI);
    }

    gpio_set(LEDDRIVER_CLK);
    gpio_clr(LEDDRIVER_CLK);
  }
  gpio_set(LEDDRIVER_LE);
  gpio_clr(LEDDRIVER_LE);

  // set up correct row
  gpio_set_or_clr(DECODER_A0, ms->row & (1 << 0));
  gpio_set_or_clr(DECODER_A1, ms->row & (1 << 1));
  gpio_set_or_clr(DECODER_A2, ms->row & (1 << 2));

  // enable
  gpio_set(DECODER_ENABLE);
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(10000, TIMER1);
  bss_canary_init();

  memset(&matrix_state, 0, sizeof(matrix_state));

  gpio_make_output(DECODER_ENABLE);
  gpio_make_output(DECODER_A0);
  gpio_make_output(DECODER_A1);
  gpio_make_output(DECODER_A2);

  gpio_make_output(LEDDRIVER_SDI);
  gpio_make_output(LEDDRIVER_OE);
  gpio_make_output(LEDDRIVER_CLK);
  gpio_make_output(LEDDRIVER_LE);

  gpio_clr(LEDDRIVER_OE);
  gpio_clr(DECODER_ENABLE);

  schedule_now((ActivationFuncPtr)update_value, &matrix_state);

  cpumon_main_loop();
}
