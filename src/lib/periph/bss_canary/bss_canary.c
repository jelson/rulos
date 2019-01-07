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

#include "core/rulos.h"
#include "periph/bss_canary/bss_canary.h"

// The linker automatically creates symbols that indicate the end of BSS.
// Unfortunately, they're named differently on different platforms.
#if defined(RULOS_AVR)
#define BSS_END_SYM __bss_end
#elif defined(SIM)
#define BSS_END_SYM _end
#else
#error "What's your bss-end auto symbol on this platform?"
#endif

extern char *BSS_END_SYM;
#define BSS_CANARY_MAGIC 0xca

void _bss_canary_update(void *data)
{
	assert(*BSS_END_SYM == BSS_CANARY_MAGIC);
	schedule_us(250000, _bss_canary_update, NULL);
}

void bss_canary_init()
{
	*BSS_END_SYM = BSS_CANARY_MAGIC;

	schedule_us(250000, _bss_canary_update, NULL);
}
