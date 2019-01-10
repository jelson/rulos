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

#if defined(BOARDCONFIG_ROCKET0)
#define NUM_LOCAL_BOARDS 8
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_NETROCKET)
#define NUM_LOCAL_BOARDS 0
#define NUM_REMOTE_BOARDS 8
#elif defined(BOARDCONFIG_ROCKET1)
#define NUM_LOCAL_BOARDS 4
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_WALLCLOCK)
#define NUM_LOCAL_BOARDS 1
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_CHASECLOCK)
#define NUM_LOCAL_BOARDS 1
#define NUM_REMOTE_BOARDS 0
#elif defined(BOARDCONFIG_DEFAULT)
#define NUM_LOCAL_BOARDS 8
#define NUM_REMOTE_BOARDS 0
#else
#error "Your app Makefile must define one of the BOARDCONFIG_xxx constants."
#include <stop>
#endif
