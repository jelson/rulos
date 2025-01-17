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

#include "periph/rocket/display_thruster_graph.h"

#include "periph/hpam/hpam.h"
#include "periph/rocket/rocket.h"

#define THRUSTER_ALPHA 130 /* update weight, out of 256 */
#define BOOSTER_ALPHA  16  /* update weight, out of 256 */

void dtg_update(DThrusterGraph *act);
void dtg_update_expwa(DThrusterGraph *dtg, int alpha, int i, uint8_t input);
void dtg_draw_one_skinny(DThrusterGraph *dtg, int i, uint8_t input,
                         SSBitmap mask);
void dtg_draw_skinny_bars(BoardBuffer *bbuf, int v, SSBitmap bm);
void dtg_draw_fat_bars(BoardBuffer *bbuf, int v, SSBitmap bm0, SSBitmap bm1);
void dtg_recv_func(MessageRecvBuffer *msg);

void dtg_init_local(DThrusterGraph *dtg, uint8_t board) {
  memset(dtg, 0, sizeof(*dtg));
  board_buffer_init(&dtg->bbuf DBG_BBUF_LABEL("thrustergraph"));
  board_buffer_push(&dtg->bbuf, board);

  schedule_us(1, (ActivationFuncPtr)dtg_update, dtg);
}

void dtg_init_remote(DThrusterGraph *dtg, uint8_t board, Network *network) {
  dtg_init_local(dtg, board);

  dtg->network = network;
  dtg->app_receiver.recv_complete_func = dtg_recv_func;
  dtg->app_receiver.port = THRUSTER_PORT;
  dtg->app_receiver.payload_capacity = sizeof(ThrusterPayload);
  dtg->app_receiver.num_receive_buffers = 1;
  dtg->app_receiver.message_recv_buffers = dtg->thruster_ring_storage;
  dtg->app_receiver.user_data = dtg;

  net_bind_receiver(dtg->network, &dtg->app_receiver);
}

void dtg_update(DThrusterGraph *dtg) {
  schedule_us(Exp2Time(16), (ActivationFuncPtr)dtg_update, dtg);

  int d;
  for (d = 0; d < NUM_DIGITS; d++) {
    dtg->bbuf.buffer[d] = 0;
  }
  dtg_draw_one_skinny(
      dtg, 0, (dtg->thruster_bits & (1 << HPAM_THRUSTER_FRONTLEFT)) ? 16 : 0,
      0b1000000);
  dtg_draw_one_skinny(dtg, 1,
                      (dtg->thruster_bits & (1 << HPAM_THRUSTER_REAR)) ? 16 : 0,
                      0b0000001);
  dtg_draw_one_skinny(
      dtg, 2, (dtg->thruster_bits & (1 << HPAM_THRUSTER_FRONTRIGHT)) ? 16 : 0,
      0b0001000);

  dtg_update_expwa(dtg, BOOSTER_ALPHA, 3,
                   (dtg->thruster_bits & (1 << HPAM_BOOSTER)) ? 16 : 0);
  dtg_draw_fat_bars(&dtg->bbuf, dtg->value[3] >> 16, 0b0000110, 0b0110000);

  board_buffer_draw(&dtg->bbuf);
}

/* values are stored as [0*8][v*8][frac*16]
 */

void dtg_update_expwa(DThrusterGraph *dtg, int alpha, int i, uint8_t input) {
  uint32_t oldValue = dtg->value[i];
  dtg->value[i] =
      ((oldValue * (256 - alpha)) + (((uint32_t)input) << 16) * alpha) >> 8;
  //	LOG("Old %08x ~ input %08x => new %08x",
  //		oldValue, (((uint32_t) input) << 16), dtg->value[i]);
}

void dtg_draw_one_skinny(DThrusterGraph *dtg, int i, uint8_t input,
                         SSBitmap mask) {
  dtg_update_expwa(dtg, THRUSTER_ALPHA, i, input);
  dtg_draw_skinny_bars(&dtg->bbuf, dtg->value[i] >> 16, mask);
}

void dtg_draw_skinny_bars(BoardBuffer *bbuf, int v, SSBitmap bm) {
  int d;
  for (d = 0; d < NUM_DIGITS; d++) {
    if (d * 2 < v) {
      bbuf->buffer[d] |= bm;
    }
  }
}

void dtg_draw_fat_bars(BoardBuffer *bbuf, int v, SSBitmap bm0, SSBitmap bm1) {
  int d;
  for (d = 0; d < NUM_DIGITS; d++) {
    if (d * 2 < v) {
      bbuf->buffer[d] |= bm0;
    }
    if (d * 2 + 1 < v) {
      bbuf->buffer[d] |= bm1;
    }
  }
}

// Update thruster state bits. Can either be called directly in a single-
// system image (unirocket), or can be called by the network-recive function
// in a distributed rocket.
void dtg_update_state(DThrusterGraph *dtg, ThrusterPayload *tp) {
  dtg->thruster_bits = tp->thruster_bits;
  //	LOG("dtg_recv_func got bits %1x!", dtg->thruster_bits & 0x7);
}

void dtg_recv_func(MessageRecvBuffer *msg) {
  DThrusterGraph *dtg = (DThrusterGraph *)msg->app_receiver->user_data;
  ThrusterPayload *tp = (ThrusterPayload *)msg->data;
  assert(msg->payload_len == sizeof(ThrusterPayload));
  dtg_update_state(dtg, tp);
  net_free_received_message_buffer(msg);
}
