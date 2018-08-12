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

#ifdef SIM

#include <assert.h>
#include "sim.h"

#define LOG(args...) do { sim_log(args); } while (0)
#define CONDSIMARG(x)	, x
#define _delay_us(x) do { usleep(x); } while(0)
#define _delay_ms(x) do { _delay_us((x) * 1000); } while(0)

#else	//!SIM

// jonh apologizes for evilly dup-declaring this here rather than
// rearranging includes to make sense.
void hardware_assert(uint16_t line);

#define assert(x)	do { if (!(x)) { hardware_assert(__LINE__); } } while(0)
#define LOG(args...)	do {} while(0)
#define CONDSIMARG(x)	/**/
#endif //SIM
