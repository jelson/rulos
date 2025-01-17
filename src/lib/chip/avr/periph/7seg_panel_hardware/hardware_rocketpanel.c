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

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>

#include "core/board_defs.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/rocket/rocket.h"

#ifndef SEGSEL0
#error \
    "Make sure your makefile defines BOARD to be the name of a board with an EPB."
#include <stophere>
#endif

bool g_boardbus_is_awake;
void boardbus_wake();
void boardbus_sleep();

//////////////////////////////////////////////////////////////////////////////

static uint8_t segmentRemapTables[4][8] = {
#define SRT_SUBU 0
    {0, 1, 2, 3, 4, 5, 6, 7},
#define SRT_SDBU 1
    // swap decimal point with segment H, which are the nodes reversed
    // when an LED is mounted upside-down
    {0, 1, 2, 3, 4, 5, 7, 6},
#define SRT_SUBD 2
    {3, 4, 5, 0, 1, 2, 6, 7},
#define SRT_SDBD 3
    {3, 4, 5, 0, 1, 2, 7, 6},
};
typedef uint8_t SegmentRemapIndex;

typedef struct {
  bool reverseDigits;
  SegmentRemapIndex segmentRemapIndices[8];
} BoardRemap;

static BoardRemap boardRemapTables[] = {
#define BRT_SOLDERED_UP_BOARD_UP 0
    {FALSE,
     {SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU,
      SRT_SUBU}},
#define BRT_SOLDERED_DN_BOARD_DN 1
    {TRUE,
     {SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD,
      SRT_SDBD}},
#define BRT_WALLCLOCK 2
    //	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU,
    // SRT_SUBU, SRT_SUBU }},
    {FALSE,
     {SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU,
      SRT_SUBU}},
#define BRT_CHASECLOCK 3
    {FALSE,
     {SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU,
      SRT_SUBU}},
};
typedef uint8_t BoardRemapIndex;

static BoardRemapIndex displayConfiguration[NUM_LOCAL_BOARDS];

static const uint16_t g_epb_delay_constant = 1;
void epb_delay() {
  uint16_t delay;
  static volatile int x;
  for (delay = 0; delay < g_epb_delay_constant; delay++) {
    x = x + 1;
  }
}

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment,
                         uint8_t onoff) {
  if (!g_boardbus_is_awake) {
    boardbus_wake();
  }
  BoardRemap *br = &boardRemapTables[displayConfiguration[board]];
  uint8_t rdigit;
  if (br->reverseDigits) {
    rdigit = digit;
  } else {
    rdigit = NUM_DIGITS - 1 - digit;
  }
  uint8_t asegment =
      segmentRemapTables[br->segmentRemapIndices[rdigit]][segment];

  gpio_set_or_clr(DIGSEL0, rdigit & (1 << 0));
  gpio_set_or_clr(DIGSEL1, rdigit & (1 << 1));
  gpio_set_or_clr(DIGSEL2, rdigit & (1 << 2));

  gpio_set_or_clr(SEGSEL0, asegment & (1 << 0));
  gpio_set_or_clr(SEGSEL1, asegment & (1 << 1));
  gpio_set_or_clr(SEGSEL2, asegment & (1 << 2));

  gpio_set_or_clr(BOARDSEL0, board & (1 << 0));
  gpio_set_or_clr(BOARDSEL1, board & (1 << 1));
  gpio_set_or_clr(BOARDSEL2, board & (1 << 2));

  /* sense reversed because cathode is common */
  gpio_set_or_clr(DATA, !onoff);

  epb_delay();

  gpio_clr(STROBE);

  epb_delay();

  gpio_set(STROBE);

  epb_delay();
}

void hal_7seg_bus_enter_sleep() { boardbus_sleep(); }

/*************************************************************************************/

void boardbus_wake() {
  gpio_make_output(BOARDSEL0);
  gpio_make_output(BOARDSEL1);
  gpio_make_output(BOARDSEL2);
  gpio_make_output(DIGSEL0);
  gpio_make_output(DIGSEL1);
  gpio_make_output(DIGSEL2);
  gpio_make_output(SEGSEL0);
  gpio_make_output(SEGSEL1);
  gpio_make_output(SEGSEL2);
  gpio_make_output(DATA);
  gpio_make_output(STROBE);
  g_boardbus_is_awake = TRUE;
}

void boardbus_sleep() {
  // Context: When a dongle is powered and the underlying display board is not,
  // we see the CPU dumping ~60mA. The hypothesis is that unpowered latches can
  // deliver current through their inputs (which would be high-Z if powered).

  // Send all other lines to high-Z to avoid current flowing.
  gpio_make_input_disable_pullup(BOARDSEL0);
  gpio_make_input_disable_pullup(BOARDSEL1);
  gpio_make_input_disable_pullup(BOARDSEL2);
  gpio_make_input_disable_pullup(DIGSEL0);
  gpio_make_input_disable_pullup(DIGSEL1);
  gpio_make_input_disable_pullup(DIGSEL2);
  gpio_make_input_disable_pullup(SEGSEL0);
  gpio_make_input_disable_pullup(SEGSEL1);
  gpio_make_input_disable_pullup(SEGSEL2);
  gpio_make_input_disable_pullup(DATA);

  // Strobe stays weakly high to prevent floating values from spuriously
  // triggering latches.
  gpio_make_input_enable_pullup(STROBE);

  g_boardbus_is_awake = FALSE;
}

void hal_init_rocketpanel() {
  // Init pins used by rocketpanel bus
  boardbus_sleep();

  // This code is static per binary; could save some code space
  // by using #ifdefs instead of dynamic code.
  for (int i = 0; i < NUM_LOCAL_BOARDS; i++) {
    displayConfiguration[i] = BRT_SOLDERED_UP_BOARD_UP;
  }

#if defined(BOARDCONFIG_ROCKET0)
  displayConfiguration[0] = BRT_WALLCLOCK;
  displayConfiguration[3] = BRT_SOLDERED_DN_BOARD_DN;
  displayConfiguration[4] = BRT_SOLDERED_DN_BOARD_DN;
#elif defined(BOARDCONFIG_ROCKET1)
  displayConfiguration[2] = BRT_SOLDERED_DN_BOARD_DN;
#elif defined(BOARDCONFIG_ROCKETDONGLENORTH)
  displayConfiguration[0] = BRT_WALLCLOCK;
  displayConfiguration[3] = BRT_SOLDERED_DN_BOARD_DN;
  displayConfiguration[4] = BRT_SOLDERED_DN_BOARD_DN;
#elif defined(BOARDCONFIG_ROCKETDONGLESOUTH)
  displayConfiguration[2] = BRT_SOLDERED_DN_BOARD_DN;
#elif defined(BOARDCONFIG_WALLCLOCK)
  displayConfiguration[0] = BRT_WALLCLOCK;
#elif defined(BOARDCONFIG_CHASECLOCK)
  displayConfiguration[0] = BRT_CHASECLOCK;
#endif
}

void debug_abuse_epb() {
  while (TRUE) {
    gpio_set(BOARDSEL0);
    gpio_clr(BOARDSEL0);
    gpio_set(BOARDSEL1);
    gpio_clr(BOARDSEL1);
    gpio_set(BOARDSEL2);
    gpio_clr(BOARDSEL2);
    gpio_set(DIGSEL0);
    gpio_clr(DIGSEL0);
    gpio_set(DIGSEL1);
    gpio_clr(DIGSEL1);
    gpio_set(DIGSEL2);
    gpio_clr(DIGSEL2);
    gpio_set(SEGSEL0);
    gpio_clr(SEGSEL0);
    gpio_set(SEGSEL1);
    gpio_clr(SEGSEL1);
    gpio_set(SEGSEL2);
    gpio_clr(SEGSEL2);
    gpio_set(DATA);
    gpio_clr(DATA);
  }
}
