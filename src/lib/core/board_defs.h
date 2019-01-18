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

#pragma once

// DBOARD(name, syms, x, y, remote_addr, remote_idx)
// name is the label for the board presented in the simulator.
// syms tells the simulator which color each LED is.
// x, y tell the simulator where to place the board display on the ncurses
// canvas. remote_addr is the address of the dongle that owns this board,
// specified as an offset from DONGLE_BASE_ADDR remote_idx is the index (board
// select configuration) of the board on that remote dongle
#define B_MISSION_CLOCK \
  DBOARD("Mission Clock", PG PG PG PG PG PG PY PY, 3, 0, 0, 0)
#define B_LUNAR_DISTANCE \
  DBOARD("Distance to Moon", PR PR PR PR PR PR PR PR, 33, 4, 1, 1)
#define B_SPEED DBOARD("Speed (fps)", PY PY PY PY PY PG PG PG, 33, 8, 2, 2)
#define B_RASTER0 DBOARD("", PR PR PR PR PR PR PR PR, 9, 12, 3, 3)
#define B_RASTER1 DBOARD("", PR PR PR PR PR PR PR PR, 9, 15, 3, 4)
#define B_RASTER2 DBOARD("", PR PR PR PR PR PR PR PR, 9, 18, 3, 5)
#define B_RASTER3 DBOARD("", PR PR PR PR PR PR PR PR, 9, 21, 3, 6)
#define B_THRUSTER_ACT \
  DBOARD("Thruster Actuation", PB PR PR PR PB PY PY PY, 3, 25, 7, 7)
#define B_AZE DBOARD("Azimuth, Elevation", PG PG PG PR PR PY PY PY, 33, 0, 8, 0)
#define B_LIQ_HYD_PRES \
  DBOARD("Liquid Hydrogen Prs", PG PG PG PG PG PG PG PG, 3, 4, 9, 1)
#define B_FLIGHTCOMPUTER0 \
  DBOARD("Flight Computer", PR PR PR PR PY PY PY PY, 17, 9, 10, 2)
#define B_FLIGHTCOMPUTER1 DBOARD("", PG PG PG PG PB PB PB PB, 17, 12, 11, 3)

#define T_ROCKET0                                                              \
  B_MISSION_CLOCK, B_LUNAR_DISTANCE, B_SPEED, B_RASTER0, B_RASTER1, B_RASTER2, \
      B_RASTER3, B_THRUSTER_ACT, B_END

#define T_ROCKET1                                              \
  B_AZE, B_LIQ_HYD_PRES, B_FLIGHTCOMPUTER0, B_FLIGHTCOMPUTER1, \
      B_NO_BOARD B_NO_BOARD B_NO_BOARD B_NO_BOARD B_END

#ifdef SIM

#define JOYSTICK_X_CHAN 3
#define JOYSTICK_Y_CHAN 2  

#else  // SIM

#include <core/hardware.h>

#if defined(BOARD_PROTO)

#define BOARDSEL0 GPIO_B2
#define BOARDSEL1 GPIO_B3
#define BOARDSEL2 GPIO_B4
#define DIGSEL0 GPIO_C0
#define DIGSEL1 GPIO_C1
#define DIGSEL2 GPIO_C2
#define SEGSEL0 GPIO_D5
#define SEGSEL1 GPIO_D6
#define SEGSEL2 GPIO_D7
#define DATA GPIO_B0
#define STROBE GPIO_B1

#define AVAILABLE_ADCS 0x38
#define ASSERT_TO_BOARD

#elif defined(BOARD_PCB10)

#define BOARDSEL0 GPIO_B2
#define BOARDSEL1 GPIO_B3
#define BOARDSEL2 GPIO_B4
#define DIGSEL0 GPIO_C0
#define DIGSEL1 GPIO_C1
#define DIGSEL2 GPIO_C2
#define SEGSEL0 GPIO_D5
#define SEGSEL1 GPIO_D6
#define SEGSEL2 GPIO_D7
#define DATA GPIO_D4
#define STROBE GPIO_B1

#define KEYPAD_ROW0 GPIO_D4
#define KEYPAD_ROW1 GPIO_B2
#define KEYPAD_ROW2 GPIO_B3
#define KEYPAD_ROW3 GPIO_B4
#define KEYPAD_COL0 GPIO_D0
#define KEYPAD_COL1 GPIO_D1
#define KEYPAD_COL2 GPIO_D2
#define KEYPAD_COL3 GPIO_D3

#define AVAILABLE_ADCS 0x38
#define ASSERT_TO_BOARD

#define JOYSTICK_X_CHAN 3
#define JOYSTICK_Y_CHAN 4

#elif defined(BOARD_PCB11)

#define BOARDSEL0 GPIO_B0
#define BOARDSEL1 GPIO_B1
#define BOARDSEL2 GPIO_B2
#define DIGSEL0 GPIO_D5
#define DIGSEL1 GPIO_D6
#define DIGSEL2 GPIO_D7
#define SEGSEL0 GPIO_B3
#define SEGSEL1 GPIO_B4
#define SEGSEL2 GPIO_B5
#define DATA GPIO_B6
#define STROBE GPIO_B7

#define KEYPAD_ROW0 DATA
#define KEYPAD_ROW1 BOARDSEL0
#define KEYPAD_ROW2 BOARDSEL1
#define KEYPAD_ROW3 BOARDSEL2
#define KEYPAD_COL0 GPIO_D0
#define KEYPAD_COL1 GPIO_D1
#define KEYPAD_COL2 GPIO_D2
#define KEYPAD_COL3 GPIO_D3

#define JOYSTICK_TRIGGER GPIO_D4
#define JOYSTICK_X_CHAN 3
#define JOYSTICK_Y_CHAN 2

#define AVAILABLE_ADCS 0x3f
#define ASSERT_TO_BOARD

#elif defined(BOARD_ROCKETAUDIO)

#define AUDIO_REGISTER_LATCH GPIO_D6
#define AUDIO_REGISTER_DATA GPIO_D5
#define AUDIO_REGISTER_SHIFT GPIO_D7
#define AVAILABLE_ADCS 0

#elif defined(BOARD_LPEM)

#define BOARDSEL0 GPIO_C2
#define BOARDSEL1 GPIO_C3
#define BOARDSEL2 GPIO_C4
#define DIGSEL0 GPIO_D2
#define DIGSEL1 GPIO_D3
#define DIGSEL2 GPIO_D5
#define SEGSEL0 GPIO_C5
#define SEGSEL1 GPIO_C6
#define SEGSEL2 GPIO_C7
#define DATA GPIO_D6
#define STROBE GPIO_D7

#define JOYSTICK_X_CHAN 3
#define JOYSTICK_Y_CHAN 2

#define JOYSTICK_TRIGGER GPIO_D4

#define AVAILABLE_ADCS 0xff
#define ASSERT_TO_BOARD

#define KEYPAD_ROW0 DATA
#define KEYPAD_ROW1 BOARDSEL0
#define KEYPAD_ROW2 BOARDSEL1
#define KEYPAD_ROW3 BOARDSEL2
#define KEYPAD_COL0 GPIO_B0
#define KEYPAD_COL1 GPIO_B1
#define KEYPAD_COL2 GPIO_B2
#define KEYPAD_COL3 GPIO_B3

#elif defined(BOARD_LPEM2)

#define JOYSTICK_TRIGGER GPIO_C7
#define JOYSTICK_X_CHAN  1
#define JOYSTICK_Y_CHAN  0
#define AVAILABLE_ADCS 0xff

#elif defined(BOARD_FLASHCARD)

#define KEYPAD_ROW0 GPIO_B7
#define KEYPAD_ROW1 GPIO_B0
#define KEYPAD_ROW2 GPIO_B1
#define KEYPAD_ROW3 GPIO_B3
#define KEYPAD_COL0 GPIO_B2
#define KEYPAD_COL1 GPIO_B4
#define KEYPAD_COL2 GPIO_B5
#define KEYPAD_COL3 GPIO_B6

#define AVAILABLE_ADCS 0xff

#define ASSERT_CUSTOM(line)                                \
  {                                                        \
    void syncdebug(uint8_t spaces, char f, uint16_t line); \
    syncdebug(0, 'A', line);                               \
  }

#elif defined(BOARD_DONGLE_REVA)

#define BOARDSEL0 GPIO_C3
#define BOARDSEL1 GPIO_D1
#define BOARDSEL2 GPIO_D3
#define DIGSEL0 GPIO_B7
#define DIGSEL1 GPIO_D6
#define DIGSEL2 GPIO_B0
#define SEGSEL0 GPIO_D0
#define SEGSEL1 GPIO_D2
#define SEGSEL2 GPIO_D4
#define DATA GPIO_C1
#define STROBE GPIO_C2

#elif defined(BOARD_DONGLE_REVB)

#define BOARDSEL0 GPIO_C3
#define BOARDSEL1 GPIO_B2
#define BOARDSEL2 GPIO_D3
#define DIGSEL0 GPIO_B7
#define DIGSEL1 GPIO_D6
#define DIGSEL2 GPIO_B0
#define SEGSEL0 GPIO_B1
#define SEGSEL1 GPIO_D2
#define SEGSEL2 GPIO_D4
#define DATA GPIO_C1
#define STROBE GPIO_C2

#elif defined(BOARD_CUSTOM)

#include "custom_board_defs.h"

#elif defined(BOARD_GENERIC)

/* do nothing */

#else

#error Unknown board type! Use GENERIC for your own target board or CUSTOM to define it in custom_board_defs.h

#endif

#ifndef AVAILABLE_ADCS
#define AVAILABLE_ADCS \
  0xff  // Good luck, board designer -- no safety check here
#endif

#endif  // SIM
