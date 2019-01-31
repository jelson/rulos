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

#include "periph/rocket/lunar_distance.h"

void lunar_distance_update(LunarDistance *ld);

#define LD_CRUISE_SPEED (-237)
#define LUNAR_DISTANCE 237674

void lunar_distance_init(LunarDistance *ld, uint8_t dist_b0,
                         uint8_t speed_b0 /*, int adc_channel*/) {
  drift_anim_init(&ld->da, 0, LUNAR_DISTANCE, 0, LUNAR_DISTANCE, 2376);
  board_buffer_init(&ld->dist_board DBG_BBUF_LABEL("dist"));
  board_buffer_push(&ld->dist_board, dist_b0);
  board_buffer_init(&ld->speed_board DBG_BBUF_LABEL("speed"));
  board_buffer_push(&ld->speed_board, speed_b0);
  da_set_velocity(&ld->da, 0);
  /*ld->adc_channel = adc_channel;
  hal_init_adc(10000);
  hal_init_adc_channel(ld->adc_channel);
  */
  schedule_us(1, (ActivationFuncPtr)lunar_distance_update, ld);
}

void lunar_distance_reset(LunarDistance *ld) {
  da_set_value(&ld->da, LUNAR_DISTANCE);
  da_set_velocity(&ld->da, 0);
}

void lunar_distance_set_velocity_256ths(LunarDistance *ld, uint16_t frac) {
  da_set_velocity(&ld->da, (((int32_t)LD_CRUISE_SPEED) * frac) >> 8);
}

void lunar_distance_update(LunarDistance *ld) {
  schedule_us(100000, (ActivationFuncPtr)lunar_distance_update, ld);

  if (da_read(&ld->da) == 0) {
    // oh. we got there. Stop things to avoid clock rollover effects.
    da_set_value(&ld->da, 0);
    // also, velocity should be zero. :v)
    da_set_velocity(&ld->da, 0);
  }

  char buf[NUM_DIGITS + 1];
  // LOG("lunar dist %d", da_read(&ld->da));
  int_to_string2(buf, NUM_DIGITS, 0, da_read(&ld->da));
  ascii_to_bitmap_str(ld->dist_board.buffer, NUM_DIGITS, buf);
  ld->dist_board.buffer[NUM_DIGITS - 4] |= SSB_DECIMAL;
  board_buffer_draw(&ld->dist_board);

  int32_t fps = -ld->da.velocity * ((uint32_t)5280 * 1024 / 1000);

  /*
          if (fps > 600)
          {
                  // let user twiddle speed with pot
                  fps += (((int16_t) hal_read_adc(ld->adc_channel)) - 512)*20;
          }
  */

  // add .03% noise to speed display
  if (fps > 100) {
    int32_t noise = (fps / 10000) * (deadbeef_rand() & 0x3);
    fps += noise;
  }

  int_to_string2(buf, NUM_DIGITS, 0, fps);
  // strcpy(&buf[6], "fs");
  ascii_to_bitmap_str(ld->speed_board.buffer, NUM_DIGITS, buf);
  if (fps > 999) {
    ld->speed_board.buffer[4] |= SSB_DECIMAL;
  }
  if (fps > 999999) {
    ld->speed_board.buffer[1] |= SSB_DECIMAL;
  }
  board_buffer_draw(&ld->speed_board);
}
