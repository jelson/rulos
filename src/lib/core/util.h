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

#include <inttypes.h>

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif

typedef uint8_t r_bool;
	// "r_bool" for ROCKET bool! ncurses and other libraries define
	// incompatible bools, so I want to avoid (rather than battle) collisions.


void util_init();
int32_t bound(int32_t v, int32_t l, int32_t h);
uint32_t isqrt(uint32_t v);

#define min(a,b)	(((a)<(b))?(a):(b))
#define max(a,b)	(((a)>(b))?(a):(b))

int int_div_with_correct_truncation(int a, int b);
int int_to_string2(char *strp, uint8_t min_width, uint8_t min_zeros, int32_t i);

extern const char hexmap[];
void debug_itoha(char *out, uint16_t i);
void itoda(char *out, uint16_t v);
	// places 6 bytes into out.
uint16_t atoi_hex(char *in);
uint16_t stack_ptr();
void debug_delay(int ms);


#ifdef AVR
# include <avr/pgmspace.h>
#else
# define PROGMEM	/**/
# define pgm_read_byte(addr)	(*((uint8_t*)addr))
#endif


