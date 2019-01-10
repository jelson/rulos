/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/board_defs.h"
#include "core/rulos.h"
#include "hardware.h"
#include "periph/uart/serial_console.h"

typedef struct {
  SerialConsole *sc;
} KeyScan;

void key_scan(KeyScan *ks);

#define KEY_SCAN_PERIOD 50000

void key_scan_init(KeyScan *ks, SerialConsole *sc) {
  ks->sc = sc;
#if 0
	gpio_make_input_enable_pullup(KEYPAD_COL0);
	gpio_make_input_enable_pullup(KEYPAD_COL1);
	gpio_make_input_enable_pullup(KEYPAD_COL2);
	gpio_make_input_enable_pullup(KEYPAD_COL3);
	gpio_make_output(KEYPAD_ROW0);
	gpio_make_output(KEYPAD_ROW1);
	gpio_make_output(KEYPAD_ROW2);
	gpio_make_output(KEYPAD_ROW3);
#endif
  hal_init_keypad();
  schedule_us(KEY_SCAN_PERIOD, (ActivationFuncPtr)key_scan, ks);
}

void key_scan(KeyScan *ks) {
  char msg[10];
  int len;

#if 0
	msg[0] = gpio_is_set(KEYPAD_COL0) ? '0' : '_';
	msg[1] = gpio_is_set(KEYPAD_COL1) ? '1' : '_';
	msg[2] = gpio_is_set(KEYPAD_COL2) ? '2' : '_';
	msg[3] = gpio_is_set(KEYPAD_COL3) ? '3' : '_';
	msg[4] = '\n';
	len = 5;
#endif

  msg[0] = hal_scan_keypad();
  msg[1] = '\n';
  len = 2;

  schedule_us(KEY_SCAN_PERIOD, (ActivationFuncPtr)key_scan, ks);

  serial_console_sync_send(ks->sc, msg, len);
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  SerialConsole sca;
  serial_console_init(&sca, NULL, NULL);

  KeyScan ks;
  key_scan_init(&ks, &sca);

  cpumon_main_loop();
  return 0;
}
