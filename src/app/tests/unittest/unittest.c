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

#include "core/queue.h"
#include "core/rulos.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"

#include <inttypes.h>

QUEUE_DECLARE(short)

#include "core/queue.mc"

QUEUE_DEFINE(short)

#define NUM_ELTS 4

void test_shortqueue() {
  uint8_t space[sizeof(shortQueue) + sizeof(short) * NUM_ELTS];
  shortQueue *sq = (shortQueue *)space;
  shortQueue_init(sq, sizeof(space));

  short in = 0, out = 0;

  while (TRUE) {
    if (deadbeef_rand() & 1) {
      if (deadbeef_rand() & 1) {
        bool rc = shortQueue_append(sq, in);
        if (rc) {
          LOG("in: %d", in);
          in++;
        } else {
          LOG("full");
        }
      } else {
        short inbuf[2];
        inbuf[0] = in;
        inbuf[1] = in + 1;
        bool rc = shortQueue_append_n(sq, inbuf, 2);
        if (rc) {
          LOG("in: %d, %d", in, in + 1);
          in += 2;
        } else {
          LOG("n-full");
        }
      }

    } else {
      short val;
      bool rc = shortQueue_pop(sq, &val);
      if (rc) {
        assert(val == out);
        LOG("        out: %d", out);
        out++;
      } else {
        LOG("        empty");
      }
    }
  }
}

void test_ring_buffer() {
  uint8_t _storage_rb[sizeof(RingBuffer) + 17 + 1];
  RingBuffer *rb = (RingBuffer *)_storage_rb;
  init_ring_buffer(rb, sizeof(_storage_rb));

  uint8_t in = 0, out = 0;

  while (TRUE) {
    if (deadbeef_rand() & 1) {
      if (ring_insert_avail(rb) > 0) {
        ring_insert(rb, in);
        LOG("in: %d", in);
        in++;
      } else {
        LOG("full");
      }
    } else {
      uint8_t ra = ring_remove_avail(rb);
      LOG("ra %d", ra);
      if (ra) {
        uint8_t val;
        val = ring_remove(rb);
        assert(val == out);
        LOG("        out: %d", out);
        out++;
      } else {
        LOG("empty");
      }
    }
  }
}

void test_later_than_case(Time a, Time b) {
  LOG("\ntesting that %"PRIu32" is later than %"PRIu32, a, b);
  if (a > b) {
    LOG("a-b=%"PRIu32, a-b);
  } else {
    LOG("b-a=%"PRIu32, b-a);
  }
  LOG("a-b later than: %d", later_than(a, b));
  assert(later_than(a, b));
}

void test_later_than() {
  test_later_than_case(1, 0);
  test_later_than_case(2000000000u, 0);
  test_later_than_case(0, 3000000000u);
  test_later_than_case(3000000001u, 3000000000u);
  test_later_than_case(1000000000u, 4000000000u);
}

void test_delta_case(Time a, Time b, int32_t result) {
  LOG("\ntesting that delta from %"PRIu32" to %"PRIu32" is %"PRIi32,
      a, b, result);
  int32_t actual_result = time_delta(a, b);
  LOG("...got %"PRIi32, actual_result);
  assert(actual_result == result);
}
  
void test_delta() {
  test_delta_case(0, 0,  0);
  test_delta_case(3000000000, 3000000000,  0);
  test_delta_case(0, 1,  1);
  test_delta_case(1, 0,  -1);
  test_delta_case(0xfffffffe, 0xffffffff, 1);
  test_delta_case(0xffffffff, 0xfffffffe, -1);
  test_delta_case(0xffffffff, 0, 1);
  test_delta_case(0xffffffff, 1000, 1001);
  test_delta_case(0, 0xffffffff, -1);
  test_delta_case(1, 0xfffffffe, -3);
}

int main() {
  rulos_hal_init();
  UartState_t uart;
  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);
  LOG("unittest up and running");

  test_later_than();
  test_delta();
  test_shortqueue();
  test_ring_buffer();
  return 0;
}
