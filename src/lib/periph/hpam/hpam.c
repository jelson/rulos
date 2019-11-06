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

#include "periph/hpam/hpam.h"

#include <stdio.h>

#include "core/rulos.h"

void hpam_update(HPAM *hpam);

#define REST_TIME_SECONDS 30
// Note that a clever pilot can work around rest times by firing for 29.9s,
// resting for 0.1s, firing again. They're here to protect valves from
// melting due to broken software.
#define REST_NONE 0
// Lights don't need rest
#define HPAM_DIGIT_0 0
#define HPAM_DIGIT_1 4

static void hpam_init_port(HPAM *hpam, HPAMIndex idx, uint8_t rest_time_secs,
                           uint8_t board, uint8_t digit, uint8_t segment) {
  hpam->hpam_ports[idx].status = FALSE;
  hpam->hpam_ports[idx].rest_time_secs = rest_time_secs;
  hpam->hpam_ports[idx].board = board;
  hpam->hpam_ports[idx].digit = digit;
  hpam->hpam_ports[idx].segment = segment;
  hpam->hpam_ports[idx].resting = FALSE;
}

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate *thrusterUpdates) {
  hpam_init_port(hpam, HPAM_HOBBS, REST_NONE, board0, HPAM_DIGIT_0, 0);
  hpam_init_port(hpam, HPAM_RESERVED, REST_NONE, board0, HPAM_DIGIT_0, 1);
  // HPAM 0 slot 2
  hpam_init_port(hpam, HPAM_LIGHTING_FLICKER, REST_NONE, board0, HPAM_DIGIT_0,
                 2);
  // HPAM 0 slot 1
  hpam_init_port(hpam, HPAM_FIVE_VOLTS, REST_NONE, board0, HPAM_DIGIT_0, 7);
  // HPAM 0 slot 3
  hpam_init_port(hpam, HPAM_THRUSTER_FRONTLEFT, REST_TIME_SECONDS, board0,
                 HPAM_DIGIT_0, 4);
  // HPAM 1 slot 3
  // cable "thruster2"
  hpam_init_port(hpam, HPAM_THRUSTER_FRONTRIGHT, REST_TIME_SECONDS, board0,
                 HPAM_DIGIT_0, 5);
  // HPAM 1 slot 2
  // calbe "thruster1"
  hpam_init_port(hpam, HPAM_THRUSTER_REAR, REST_TIME_SECONDS, board0,
                 HPAM_DIGIT_0, 6);
  // HPAM 1 slot 1
  // cable "thruster0"
  hpam_init_port(hpam, HPAM_BOOSTER, REST_TIME_SECONDS, board0, HPAM_DIGIT_0,
                 3);
  // HPAM 1 slot 3
  hpam->thrusterPayload.thruster_bits = 0;
  hpam->thrusterUpdates = thrusterUpdates;

  board_buffer_init(&hpam->bbuf DBG_BBUF_LABEL("hpam"));
  board_buffer_set_alpha(&hpam->bbuf, 0x88);  // hard-code hpam digits 0,4
  board_buffer_push(&hpam->bbuf, board0);

  int idx;
  for (idx = 0; idx < HPAM_END; idx++) {
    hpam_set_port(hpam, (HPAMIndex)idx, TRUE);  // force a state change
    hpam_set_port(hpam, (HPAMIndex)idx, FALSE);
  }

  schedule_us(1, (ActivationFuncPtr)hpam_update, hpam);
}

void hpam_update(HPAM *hpam) {
  schedule_us(1000000, (ActivationFuncPtr)hpam_update, hpam);

  // check that no valve stays open more than max_time.
  int idx;
  for (idx = 0; idx < HPAM_END; idx++) {
    HPAMPort *port = &hpam->hpam_ports[idx];
    if (port->status && !port->resting && port->rest_time_secs > 0 &&
        later_than(clock_time_us(), port->expire_time)) {
      hpam_set_port(hpam, (HPAMIndex)idx, FALSE);

      port->resting = TRUE;
      // 33% duty cycle: rest expires two timeouts from now
      port->expire_time =
          clock_time_us() + 2 * ((Time)port->rest_time_secs * 1000000);
      LOG("RESTING HPAM idx %d", idx);
    }
    if (port->resting && later_than(clock_time_us(), port->expire_time)) {
      port->resting = FALSE;
      LOG("reactivating HPAM idx %d", idx);
    }
  }
}

void hpam_set_port(HPAM *hpam, HPAMIndex idx, r_bool status) {
  assert(0 <= idx && idx < HPAM_END);

  HPAMPort *port = &hpam->hpam_ports[idx];
  if (port->status == status) {
    return;
  }
  if (port->resting) {
    // can't fiddle with it.
    return;
  }

  port->status = status;

  // drive the HPAM through the LED latch
  SSBitmap mask = (1 << port->segment);
  hpam->bbuf.buffer[port->digit] =
      (hpam->bbuf.buffer[port->digit] & ~mask) | (status ? 0 : mask);
  //	LOG("idx %d status %d digit now %x",
  //		idx, status, hpam->bbuf.buffer[port->digit]);

#define RAW_PROGRAM 0
#if RAW_PROGRAM
  hal_program_segment(port->board, port->digit, port->segment, status);
#else   // RAW_PROGRAM
  board_buffer_draw(&hpam->bbuf);
#endif  // RAW_PROGRAM

  // update expire time (only matters for sets)
  port->expire_time = clock_time_us() + ((Time)port->rest_time_secs * 1000000);

  // Forward the notice to listeners
  uint8_t thruster_bits_mask = (1 << idx);
  hpam->thrusterPayload.thruster_bits =
      (hpam->thrusterPayload.thruster_bits & ~thruster_bits_mask) |
      (status ? thruster_bits_mask : 0);
  ThrusterUpdate *tu = hpam->thrusterUpdates;
  while (tu[0].func != NULL) {
    tu[0].func(tu[0].data, &hpam->thrusterPayload);
    tu++;
  }

#if SIM
  if (idx == HPAM_LIGHTING_FLICKER) {
    sim_display_light_status(status);
  }
#endif  // SIM
}

r_bool hpam_get_port(HPAM *hpam, HPAMIndex idx) {
  assert(0 <= idx && idx < HPAM_END);

  HPAMPort *port = &hpam->hpam_ports[idx];
  return port->status;
}
