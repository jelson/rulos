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

#ifndef __util_h__
#define __util_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#include <inttypes.h>

#define FALSE 0
#define TRUE 1
typedef uint8_t r_bool;
	// "r_bool" for ROCKET bool! ncurses and other libraries define
	// incompatible bools, so I want to avoid (rather than battle) collisions.

#ifdef SIM

#include <stdio.h>
#include <assert.h>

#define say(x)	{ fprintf(stderr, "say: %s\n", x); }
extern FILE *logfp;
#define LOGF(x)	{ fprintf x; fflush(logfp); }
#define CONDSIMARG(x)	, x

#define PROGMEM	/**/
#define pgm_read_byte(addr)	(*((uint8_t*)addr))

#else	//!SIM

#include <avr/pgmspace.h>

// jonh apologizes for evilly dup-declaring this here rather than
// rearranging includes to make sense.
void hardware_assert(uint16_t line);

#define assert(x)	{ if (!(x)) { hardware_assert(__LINE__); } }
#define LOGF(x)	{}
#define CONDSIMARG(x)	/**/
#endif //SIM

void util_init();
int32_t bound(int32_t v, int32_t l, int32_t h);
uint32_t isqrt(uint32_t v);

#define min(a,b)	(((a)<(b))?(a):(b))
#define max(a,b)	(((a)>(b))?(a):(b))

int int_div_with_correct_truncation(int a, int b);

extern char hexmap[16];
void debug_itoha(char *out, uint16_t i);
void itoda(char *out, uint16_t v);
	// places 6 bytes into out.
uint16_t atoi_hex(char *in);
void debug_msg_hex(uint8_t board, char *m, uint16_t hex);
uint16_t stack_ptr();
void debug_delay(int ms);
#define debug_plod(m)	{ debug_msg_hex(0, m, __LINE__); debug_delay(250); }
void board_debug_msg(uint16_t line);


#endif // _util_h
