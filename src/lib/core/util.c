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

#include "core/util.h"

int32_t bound(int32_t v, int32_t l, int32_t h) {
  if (v < l) {
    return l;
  }
  if (v > h) {
    return h;
  }
  return v;
}

uint32_t isqrt(uint32_t v) {
  // http://www.embedded.com/98/9802fe2.htm, listing 2
  uint32_t square = 1;
  uint32_t delta = 3;
  while (square < v) {
    square += delta;
    delta += 2;
  }
  return (delta / 2 - 1);
}

int int_div_with_correct_truncation(int a, int b) {
  if (b < 0) {
    a = -a;
    b = -b;
  }  // b is positive
  if (a > 0) {
    return a / b;
  } else {
    int tmp = (-a) / b;
    if (tmp * b < -a) {
      // got truncated; round *up*
      tmp += 1;
    }
    return -tmp;
  }
}

const char hexmap[] = "0123456789ABCDEF";

void debug_itoha(char *out, uint16_t i) {
  out[0] = hexmap[(i >> (3 * 4)) & 0xf];
  out[1] = hexmap[(i >> (2 * 4)) & 0xf];
  out[2] = hexmap[(i >> (1 * 4)) & 0xf];
  out[3] = hexmap[(i >> (0 * 4)) & 0xf];
  out[4] = '\0';
}

// places 6 bytes into out.
void itoda(char *out, uint16_t v) {
  out[5] = '\0';
  uint8_t i;
  for (i = 0; i < 5; i++) {
    uint16_t frac = v / 10;
    out[4 - i] = '0' + (v - frac * 10);
    v = frac;
  }
}

uint16_t atoi_hex(char *in) {
  uint16_t v = 0;
  uint8_t u;
  while (1) {
    char c = (*in);
    if (c >= '0' && c <= '9') {
      u = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      u = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      u = c - 'a' + 10;
    } else {
      break;
    }
    v = (v << 4) | u;
    in++;
  }
  return v;
}

int int_to_string2(char *strp, uint8_t min_width, uint8_t min_zeros,
                   int32_t i) {
  int c = 0;
  int neg = 0;
  char *ptr = 0;

  // LOG("i %d", );
  if (strp != 0) {
    int ct = int_to_string2(0, min_width, min_zeros, i);
    ptr = strp + ct - 1;
    ptr[1] = '\0';
  }

  if (i < 0) {
    i = -i;
    neg = 1;
  }
  if (i == 0) {
    if (strp != 0) {
      ptr[-c] = '0';
    }
    c += 1;
  } else {
    while (i > 0) {
      if (strp != 0) {
        ptr[-c] = '0' + i % 10;
      }
      c += 1;
      i /= 10;
    }
  }
  while (c < min_zeros) {
    if (strp != 0) {
      ptr[-c] = '0';
    }
    c += 1;
  }
  if (neg) {
    if (strp != 0) {
      ptr[-c] = '-';
    }
    c += 1;
  }
  while (c < min_width) {
    if (strp != 0) {
      ptr[-c] = ' ';
    }
    c += 1;
  }
  // LOG("  ct %d str %s", c, strp);
  return c;
}

#if 0
uint16_t stack_ptr()
{
	uint8_t a, *b=&a;
	return (unsigned) b;
}
#endif

void debug_delay(int ms) {
  volatile int i, j;
  for (i = 0; i < ms; i++) {
    for (j = 0; j < 1000; j++) {
    }
  }
}
