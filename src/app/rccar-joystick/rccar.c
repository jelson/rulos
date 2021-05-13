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

#include "core/logging.h"
#include "core/rulos.h"

typedef struct {
  bool state;
} Blink;

void blinkfunc(Blink* blink) {
  gpio_set_or_clr(GPIO_C5, blink->state & 1);
  blink->state = 1 - blink->state;
  schedule_us(100000, (ActivationFuncPtr)blinkfunc, blink);
}

#define JOYPERIOD 100000  // later 20ms

typedef struct {
  uint8_t x_adc_channel;
  uint8_t y_adc_channel;
} Joy;

void init_joy(Joy* joy) {
  hal_init_adc(JOYPERIOD);
  joy->x_adc_channel = 0;
  joy->y_adc_channel = 1;
  hal_init_adc_channel(joy->x_adc_channel);
  hal_init_adc_channel(joy->y_adc_channel);
}

void joyfunc(Joy* joy) {
  int16_t xval = hal_read_adc(joy->x_adc_channel);
  int16_t yval = hal_read_adc(joy->y_adc_channel);
  LOG("x joy %4d y %4d", xval, yval);
  schedule_us(JOYPERIOD, (ActivationFuncPtr)joyfunc, joy);
}

/************************************************************************************/
/************************************************************************************/

int main() {
  hal_init();

  UartState_t uart;
  uart_init(&uart, /*uart_id=*/0, 38400, TRUE);
  log_bind_uart(&uart);
  LOG("log running");

  init_clock(10000, TIMER1);
  gpio_make_output(GPIO_C5);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  Blink blink;
  blink.state = 1;
  schedule_us(100000, (ActivationFuncPtr)blinkfunc, &blink);

  Joy joy;
  init_joy(&joy);
  schedule_us(JOYPERIOD, (ActivationFuncPtr)joyfunc, &joy);

  // install_handler(ADC, adc_handler);

  cpumon_main_loop();

  return 0;
}
