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

#if !defined(DBOARD)
#error "display_tree.ch is meant to be included after defining DBOARD to tell it how to use the DBOARD fields."
#include <stophere>
#endif

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
  DBOARD("Distance to Moon", PR PR PR PR PR PR PR PR, 13, 4, 1, 1)
#define B_SPEED DBOARD("Speed (fps)", PY PY PY PY PY PG PG PG, 13, 8, 2, 2)
#define B_RASTER0 DBOARD("", PR PR PR PR PR PR PR PR, 9, 12, 3, 3)
#define B_RASTER1 DBOARD("", PR PR PR PR PR PR PR PR, 9, 15, 3, 4)
#define B_RASTER2 DBOARD("", PR PR PR PR PR PR PR PR, 9, 18, 3, 5)
#define B_RASTER3 DBOARD("", PR PR PR PR PR PR PR PR, 9, 21, 3, 6)
#define B_THRUSTER_ACT \
  DBOARD("Thruster Actuation", PB PR PR PR PB PY PY PY, 3, 25, 7, 7)
#define B_AZE DBOARD("Azimuth, Elevation", PG PG PG PR PR PY PY PY, 58, 0, 8, 0)
#define B_LIQ_HYD_PRES \
  DBOARD("Liquid Hydrogen Prs", PG PG PG PG PG PG PG PG, 58, 4, 9, 1)
#define B_FLIGHTCOMPUTER0 \
  DBOARD("Flight Computer", PR PR PR PR PY PY PY PY, 61, 9, 10, 2)
#define B_FLIGHTCOMPUTER1 DBOARD("", PG PG PG PG PB PB PB PB, 61, 12, 11, 3)

#define T_ROCKET0                                                              \
  B_MISSION_CLOCK, B_LUNAR_DISTANCE, B_SPEED, B_RASTER0, B_RASTER1, B_RASTER2, \
      B_RASTER3, B_THRUSTER_ACT, B_END

#define T_ROCKET1                                              \
  B_AZE, B_LIQ_HYD_PRES, B_FLIGHTCOMPUTER0, B_FLIGHTCOMPUTER1, \
      B_NO_BOARD B_NO_BOARD B_NO_BOARD B_NO_BOARD B_END

#define T_UNIROCKET \
  B_MISSION_CLOCK, B_LUNAR_DISTANCE, B_SPEED, B_RASTER0, B_RASTER1, B_RASTER2, \
      B_RASTER3, B_THRUSTER_ACT, \
  B_AZE, B_LIQ_HYD_PRES, B_FLIGHTCOMPUTER0, B_FLIGHTCOMPUTER1, \
  B_END

#define B_WALLCLOCK DBOARD("Clock", PG PG PG PG PG PG PB PB, 15, 0, 0, 0)
#define T_WALLCLOCK B_WALLCLOCK, B_END

#define B_CHASECLOCK DBOARD("Chase", PG PG PG PG PY PY PY PY, 15, 0, 0, 0)
#define T_CHASECLOCK B_CHASECLOCK, B_END

#define B_DEFAULT0 DBOARD("board0", PG PG PG PG PG PG PG PG, 15, 0, 0, 0)
#define B_DEFAULT1 DBOARD("board1", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT2 DBOARD("board2", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT3 DBOARD("board3", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT4 DBOARD("board4", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT5 DBOARD("board5", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT6 DBOARD("board6", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define B_DEFAULT7 DBOARD("board7", PG PG PG PG PG PG PG PG, 15, 4, 0, 0)
#define T_DEFAULT \
    B_DEFAULT0, B_DEFAULT1, B_DEFAULT2, B_DEFAULT3, \
    B_DEFAULT4, B_DEFAULT5, B_DEFAULT6, B_DEFAULT7, B_END

#if defined(BOARDCONFIG_ROCKET0) || defined(BOARDCONFIG_NETROCKET)
#define ROCKET_TREE T_ROCKET0
#elif defined(BOARDCONFIG_ROCKET1)
#define ROCKET_TREE T_ROCKET1
#elif defined(BOARDCONFIG_UNIROCKET) || defined(BOARDCONFIG_UNIROCKET_LOCALSIM)
#define ROCKET_TREE T_UNIROCKET
#elif defined(BOARDCONFIG_ROCKETSOUTHBRIDGE)
#define ROCKET_TREE T_EMPTY
#elif defined(BOARDCONFIG_WALLCLOCK)
#define ROCKET_TREE T_WALLCLOCK
#elif defined(BOARDCONFIG_CHASECLOCK)
#define ROCKET_TREE T_CHASECLOCK
#elif defined(BOARDCONFIG_DEFAULT)
#define ROCKET_TREE T_DEFAULT
#else
#error "Unknown board-tree config (consider BOARDCONFIG_DEFAULT)"
#include <stophere>
#endif

