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

#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

void util_init();
int32_t bound(int32_t v, int32_t l, int32_t h);
uint32_t isqrt(uint32_t v);

#define r_min(a, b) (((a) < (b)) ? (a) : (b))
#define r_max(a, b) (((a) > (b)) ? (a) : (b))

int int_div_with_correct_truncation(int a, int b);
int int_to_string2(char *strp, uint8_t min_width, uint8_t min_zeros, int32_t i);

extern const char hexmap[];
void debug_itoha(char *out, uint16_t i);
void itoda(char *out, uint16_t v);
// places 6 bytes into out.
uint16_t atoi_hex(char *in);
uint16_t stack_ptr();
void debug_delay(int ms);

// weird preprocessor magic to turn a #define into a string
#define STRINGIFY2(x)  #x
#define STRINGIFY(x)  STRINGIFY2(x)

#ifdef AVR
#include <avr/pgmspace.h>
#else
#define PROGMEM             /**/
#define pgm_read_byte(addr) (*((uint8_t *)addr))
#endif
