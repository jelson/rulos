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

#include "core/rulos.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

#ifndef SIM
#include "core/hardware.h"
#endif

//////////////////////////////////////////////////////////////////////////////

// output appears on pin PC0. (other PC pins are divided by 2^k)
// Use it to switch the gate of a mosfet.
// (The computer sends 2V; a transistor might work, too.)

// Invocation arguments
#define CUR_MILES  26.9
#define GOAL_MILES (96 + 274)

// Configuration parameters
#define CM_PER_REVOLUTION (216) /* wheel diameter in cm */
// Measurement indicates that 69mph is about as fast as the
// calculator can tolerate.
#define MPH (69)

// Math.
#define CM_PER_MILE  (2.54 * 12 * 5280)
#define REV_PER_MILE (CM_PER_MILE / CM_PER_REVOLUTION)

#define HR_PER_MILE     (1.0 / MPH)
#define HR_PER_REV      (HR_PER_MILE / REV_PER_MILE)
#define SEC_PER_REV     (HR_PER_REV * 3600)
#define REV_PERIOD      (1000000 * SEC_PER_REV)
#define REV_HALF_PERIOD (REV_PERIOD / 2)

#define RUN_MILES (GOAL_MILES - CUR_MILES)
#define RUN_REVS  RUN_MILES *REV_PER_MILE

//////////////////////////////////////////////////////////////////////////////

#define SYNCDEBUG() syncdebug(0, 'T', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line) {
  char buf[32], hexbuf[6];
  int i;
  for (i = 0; i < spaces; i++) {
    buf[i] = ' ';
  }
  buf[i++] = f;
  buf[i++] = ':';
  buf[i++] = '\0';
  if (f >= 'a')  // lowercase -> hex value; so sue me
  {
    debug_itoha(hexbuf, line);
  } else {
    itoda(hexbuf, line);
  }
  strcat(buf, hexbuf);
  LOG("%s", buf);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  uint8_t val;
  Time period;
  uint32_t revs_remaining;
} BlinkAct;

void _update_blink(BlinkAct *ba) {
  if (ba->revs_remaining > 0) {
    schedule_us(ba->period, (ActivationFuncPtr)_update_blink, ba);
  }

  ba->val += 1;
#if !SIM
  PORTC = ba->val;
#endif
  if (ba->val & 1) {
    ba->revs_remaining -= 1;
  }
}

void blink_init(BlinkAct *ba) {
  ba->val = 0;
  ba->period = REV_HALF_PERIOD;
  ba->revs_remaining = RUN_REVS;

#if !SIM
  gpio_make_output(GPIO_C0);
  gpio_make_output(GPIO_C1);
  gpio_make_output(GPIO_C2);
  gpio_make_output(GPIO_C3);
  gpio_make_output(GPIO_C4);
  gpio_make_output(GPIO_C5);
  gpio_make_output(GPIO_C6);
  gpio_make_output(GPIO_C7);
#endif  // !SIM

  schedule_us(100000, (ActivationFuncPtr)_update_blink, ba);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  UartState_t uart;
  LineReader_t linereader;
  BlinkAct ba;
} Shell;

void shell_func(UartState_t *uart, void *data, char *line);
void print_func(Shell *shell);

void shell_init(Shell *shell) {
  uart_init(&shell->uart, 0, 38400, true);
  log_bind_uart(&shell->uart);
  linereader_init(&shell->linereader, &shell->uart, shell_func, shell);
  print_func(shell);
  blink_init(&shell->ba);
  SYNCDEBUG();
}

void print32(uint32_t v) {
  syncdebug(2, 'h', (v) >> 16);
  syncdebug(6, 'l', (v)&0xffff);
}

void print_func(Shell *shell) {
  LOG("period");
  print32(shell->ba.period);
  LOG("revs_remaining");
  print32(shell->ba.revs_remaining);
  LOG("REV_PER_MILE");
  print32(REV_PER_MILE);
  LOG("REV_PERIOD");
  print32(REV_PERIOD);
  LOG("REV_HALF_PERIOD");
  print32(REV_HALF_PERIOD);
}

void shell_func(UartState_t *uart, void *data, char *line) {
  Shell *shell = (Shell *)data;

  if (strncmp(line, "per ", 4) == 0) {
    SYNCDEBUG();
    shell->ba.period = atoi_hex(&line[4]);
  } else if (strcmp(line, "print") == 0) {
    print_func(shell);
  }
  line++;
  LOG("ok");
}

//////////////////////////////////////////////////////////////////////////////

int main() {
  hal_init();
  init_clock(1000, TIMER1);

  Shell shell;
  shell_init(&shell);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase
  cpumon_main_loop();
  return 0;
}
