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

#include "periph/rocket/display_thruster_graph.h"
#include "periph/hpam/hpam.h"
#include "periph/rocket/rocket.h"

#define THRUSTER_ALPHA 130 /* update weight, out of 256 */
#define BOOSTER_ALPHA 16   /* update weight, out of 256 */

void dtg_update(DThrusterGraph *act);
void dtg_update_expwa(DThrusterGraph *dtg, int alpha, int i, uint8_t input);
void dtg_draw_one_skinny(DThrusterGraph *dtg, int i, uint8_t input,
                         SSBitmap mask);
void dtg_draw_skinny_bars(BoardBuffer *bbuf, int v, SSBitmap bm);
void dtg_draw_fat_bars(BoardBuffer *bbuf, int v, SSBitmap bm0, SSBitmap bm1);
void dtg_recv_func(RecvSlot *recvSlot, uint8_t payload_len);

void dtg_init(DThrusterGraph *dtg, uint8_t board, Network *network) {
  board_buffer_init(&dtg->bbuf DBG_BBUF_LABEL("thrustergraph"));
  board_buffer_push(&dtg->bbuf, board);

  dtg->network = network;
  dtg->recvSlot.func = dtg_recv_func;
  dtg->recvSlot.port = THRUSTER_PORT;
  dtg->recvSlot.payload_capacity = sizeof(ThrusterPayload);
  dtg->recvSlot.msg_occupied = FALSE;
  dtg->recvSlot.msg = (Message *)dtg->thruster_message_storage;
  dtg->recvSlot.user_data = dtg;

  net_bind_receiver(dtg->network, &dtg->recvSlot);

  schedule_us(1, (ActivationFuncPtr)dtg_update, dtg);
}

void dtg_update(DThrusterGraph *dtg) {
  schedule_us(Exp2Time(16), (ActivationFuncPtr)dtg_update, dtg);

  int d;
  for (d = 0; d < NUM_DIGITS; d++) {
    dtg->bbuf.buffer[d] = 0;
  }
  dtg_draw_one_skinny(
      dtg, 0, (dtg->thruster_bits & (1 << hpam_thruster_frontleft)) ? 16 : 0,
      0b1000000);
  dtg_draw_one_skinny(dtg, 1,
                      (dtg->thruster_bits & (1 << hpam_thruster_rear)) ? 16 : 0,
                      0b0000001);
  dtg_draw_one_skinny(
      dtg, 2, (dtg->thruster_bits & (1 << hpam_thruster_frontright)) ? 16 : 0,
      0b0001000);

  dtg_update_expwa(dtg, BOOSTER_ALPHA, 3,
                   (dtg->thruster_bits & (1 << hpam_booster)) ? 16 : 0);
  dtg_draw_fat_bars(&dtg->bbuf, dtg->value[3] >> 16, 0b0000110, 0b0110000);

  board_buffer_draw(&dtg->bbuf);
}

/* values are stored as [0*8][v*8][frac*16]
 */

void dtg_update_expwa(DThrusterGraph *dtg, int alpha, int i, uint8_t input) {
  uint32_t oldValue = dtg->value[i];
  dtg->value[i] =
      ((oldValue * (256 - alpha)) + (((uint32_t)input) << 16) * alpha) >> 8;
  //	LOG("Old %08x ~ input %08x => new %08x\n",
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

void dtg_recv_func(RecvSlot *recvSlot, uint8_t payload_len) {
  DThrusterGraph *dtg = (DThrusterGraph *)recvSlot->user_data;
  ThrusterPayload *tp = (ThrusterPayload *)recvSlot->msg->data;
  assert(payload_len == sizeof(ThrusterPayload));
  dtg->thruster_bits = tp->thruster_bits;
  dtg->recvSlot.msg_occupied = FALSE;
  //	LOG("dtg_recv_func got bits %1x!\n", dtg->thruster_bits & 0x7);
}
