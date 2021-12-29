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

#include "core/bss_canary.h"

#include "core/rulos.h"

#if defined(RULOS_ESP32)

// The ESP32 allocates task stacks on the heap, so there's no easy way to tell
// if any of them have overflowed. We just disable bss_canary on ESP32.
void bss_canary_init() {
}

#else  // RULOS_ESP32

// The linker automatically creates symbols that indicate the end of BSS.
// Unfortunately, they're named differently on different platforms.
#if defined(RULOS_AVR)
#define BSS_END_SYM _end
#elif defined(RULOS_ARM)
#define BSS_END_SYM __bss_end__
#elif defined(SIM)
#define BSS_END_SYM _end
#else
#error "What's your bss-end auto symbol on this platform?"
#endif

extern uint8_t BSS_END_SYM;
#define BSS_CANARY_MAGIC 0xca

static void bss_canary_update(void *data) {
  assert(BSS_END_SYM == BSS_CANARY_MAGIC);
  schedule_us(250000, bss_canary_update, NULL);
}

void bss_canary_init() {
  BSS_END_SYM = BSS_CANARY_MAGIC;

  schedule_us(250000, bss_canary_update, NULL);
}

#endif  // RULOS_ESP32
