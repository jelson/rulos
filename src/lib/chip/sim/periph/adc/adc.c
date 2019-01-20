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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "core/curses.h"
#include "core/keystrokes.h"
#include "core/time.h"
#include "core/util.h"

/********************** adc (&joystick) simulator *********************/

#define NUM_ADC 6

r_bool sim_adc_keystroke_handler(char k);

static WINDOW *adc_input_window = NULL;
static int adc_input_channel = 0;
uint16_t adc[NUM_ADC];

static void draw_adc_input_window() {
  mvwprintw(adc_input_window, 1, 1, "ADC Channel %d:", adc_input_channel);
  mvwprintw(adc_input_window, 2, 1, "%4d", adc[adc_input_channel]);
  keypad(curses_get_window(), 1);
  wrefresh(adc_input_window);
}

static void adc_simulator_input(int c) {
  switch (c) {
    case KEY_RIGHT:
      adc_input_channel++;
      break;
    case KEY_LEFT:
      adc_input_channel--;
      break;
    case KEY_UP:
      adc[adc_input_channel] += 5;
      break;
    case KEY_DOWN:
      adc[adc_input_channel] -= 5;
      break;
    case KEY_PPAGE:
      adc[adc_input_channel] += 100;
      break;
    case KEY_NPAGE:
      adc[adc_input_channel] -= 100;
      break;
  }

  adc_input_channel = max(0, min(NUM_ADC, adc_input_channel));
  adc[adc_input_channel] = max(0, min(1023, adc[adc_input_channel]));

  draw_adc_input_window();
}

static void adc_simulator_stop() {
  keypad(curses_get_window(), 0);
  delwin(adc_input_window);
}

static void adc_simulator_start() {
  // set up handlers
  sim_install_modal_handler(adc_simulator_input, adc_simulator_stop);

  // create input window
  adc_input_window = newwin(4, 17, 2, 27);
  box(adc_input_window, ACS_VLINE, ACS_HLINE);
  draw_adc_input_window();
}

void hal_init_adc(Time scan_period) {
  sim_maybe_init_and_register_keystroke_handler(sim_adc_keystroke_handler);
}

r_bool sim_adc_keystroke_handler(char c) {
  switch (c) {
    case 'i':
      // Open modal ADC input dialog.
      adc_simulator_start();
      return true;

    // Shortcuts for joystick.
    case '!':        // center ADC
      adc[2] = 512;  // y
      adc[3] = 512;  // x
      return true;
    case '@':       // back-left
      adc[2] = 1023;  // y
      adc[3] = 11;  // x
      return true;
    case '#':         // fwd
      adc[2] = 11;  // y
      adc[3] = 512;   // x
      return true;
    case '$':         // back-right
      adc[2] = 1023;    // y
      adc[3] = 1023;  // x
      return true;

    default:
      return false;
  }
}

void hal_init_adc_channel(uint8_t idx) { adc[idx] = 512; }

uint16_t hal_read_adc(uint8_t idx) { return adc[idx]; }
