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
#include <stdio.h>
#include <string.h>

#include "periph/rocket/rocket.h"
#include "periph/joystick/joystick.h"
#include "periph/rocket/display_thrusters.h"

/************************************************************************************/
/************************************************************************************/


int main()
{
  HPAM hpam;
  ThrusterUpdate *thrusterUpdate[4];

  util_init();
  hal_init(bc_rocket0);
  init_clock(10000, TIMER1);
  memset(thrusterUpdate, 0, sizeof(thrusterUpdate));
  init_hpam(&hpam, 7, thrusterUpdate);
  board_buffer_module_init();
  
  ThrusterState_t ts;
  thrusters_init(&ts, 7, 3, 2, &hpam, NULL);
  
  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();
  return 0;
}

