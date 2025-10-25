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

#include "periph/rocket/breakout.h"

#include "core/rulos.h"
#include "periph/audio/sound.h"
#include "periph/rasters/rasters.h"

#define BRK_FREQ          32  // Animation frequency, Hz
#define PADDLE_SPEED      2   // Pixels per tick
#define BALL_SPEED_X      3   // Base horizontal speed (scaled)
#define BALL_SPEED_Y      3   // Base vertical speed (scaled)
#define BRICK_POINTS      10  // Points per brick
#define BRICKS_START_Y    2   // Y position where bricks start
#define BRICK_GAP         1   // Gap between bricks

// Scaled macro
#define BS(v) ((v) << BRK_SCALE2)

void breakout_update(Breakout *brk);
UIEventDisposition breakout_event_handler(UIEventHandler *raw_handler,
                                          UIEvent evt);
void breakout_paint_once(Breakout *brk);
void breakout_reset_game(Breakout *brk);
void breakout_init_bricks(Breakout *brk);
void breakout_tick(Breakout *brk);
void breakout_waiting_tick(Breakout *brk);
void breakout_playing_tick(Breakout *brk);
void breakout_dying_tick(Breakout *brk);
void breakout_game_over_tick(Breakout *brk);
void breakout_move_paddle(Breakout *brk);
void breakout_launch_ball(Breakout *brk);
void breakout_update_ball(Breakout *brk);
void breakout_check_brick_collisions(Breakout *brk);
void breakout_paint_paddle(Breakout *brk, RectRegion *rrect);
void breakout_paint_ball(Breakout *brk, RectRegion *rrect);
void breakout_paint_bricks(Breakout *brk, RectRegion *rrect);
void breakout_paint_explosion(Breakout *brk, RectRegion *rrect);
void breakout_start_explosion(Breakout *brk, int8_t x, int8_t y);

void breakout_init(Breakout *brk, Screen4 *s4, AudioClient *audioClient,
                   JoystickState_t *joystick, uint8_t lives_boardnum,
                   uint8_t score_boardnum) {
  brk->s4 = s4;
  brk->handler.func = (UIEventHandlerFunc)breakout_event_handler;
  brk->handler.breakout = brk;
  brk->audioClient = audioClient;
  brk->joystick = joystick;
  brk->lives_boardnum = lives_boardnum;
  board_buffer_init(&brk->lives_bbuf DBG_BBUF_LABEL("breakout lives"));
  brk->score_boardnum = score_boardnum;
  board_buffer_init(&brk->score_bbuf DBG_BBUF_LABEL("breakout score"));
  brk->focused = FALSE;
  breakout_reset_game(brk);

  schedule_us(1, (ActivationFuncPtr)breakout_update, brk);
}

void breakout_reset_game(Breakout *brk) {
  brk->mode = BRK_WAITING;
  brk->lives = 3;
  brk->score = 0;
  brk->explosion_radius = 0;
  brk->last_trigger_state = FALSE;

  brk->paddle_x = (BRK_CANVAS_W - BRK_PADDLE_W) / 2;

  // Ball starts on paddle
  brk->ball_x = BS(brk->paddle_x + BRK_PADDLE_W / 2 - BRK_BALL_SIZE / 2);
  brk->ball_y = BS(BRK_CANVAS_H - BRK_PADDLE_H - BRK_BALL_SIZE - 1);
  brk->ball_dx = 0;
  brk->ball_dy = 0;

  breakout_init_bricks(brk);
}

void breakout_init_bricks(Breakout *brk) {
  brk->bricks_remaining = BRK_MAX_BRICKS;
  for (int i = 0; i < BRK_MAX_BRICKS; i++) {
    brk->bricks[i] = TRUE;
  }
}

void breakout_update(Breakout *brk) {
  schedule_us(1000000 / BRK_FREQ, (ActivationFuncPtr)breakout_update, brk);
  if (brk->focused) {
    breakout_tick(brk);
  }
  breakout_paint_once(brk);
}

void breakout_tick(Breakout *brk) {
  switch (brk->mode) {
    case BRK_WAITING:
      breakout_waiting_tick(brk);
      break;
    case BRK_PLAYING:
      breakout_playing_tick(brk);
      break;
    case BRK_DYING:
      breakout_dying_tick(brk);
      break;
    case BRK_GAME_OVER:
    case BRK_VICTORY:
      breakout_game_over_tick(brk);
      break;
  }
}

void breakout_waiting_tick(Breakout *brk) {
  breakout_move_paddle(brk);

  // Keep ball attached to paddle
  brk->ball_x = BS(brk->paddle_x + BRK_PADDLE_W / 2 - BRK_BALL_SIZE / 2);

  // Check for trigger press to launch
  bool trigger_now = (brk->joystick->state & JOYSTICK_STATE_TRIGGER) != 0;
  if (trigger_now && !brk->last_trigger_state) {
    breakout_launch_ball(brk);
  }
  brk->last_trigger_state = trigger_now;
}

void breakout_playing_tick(Breakout *brk) {
  breakout_move_paddle(brk);
  breakout_update_ball(brk);
}

void breakout_dying_tick(Breakout *brk) {
  brk->explosion_radius++;
  if (brk->explosion_radius > 8) {
    brk->explosion_radius = 0;
    if (brk->lives > 0) {
      // Reset for next life
      brk->mode = BRK_WAITING;
      brk->paddle_x = (BRK_CANVAS_W - BRK_PADDLE_W) / 2;
      brk->ball_x = BS(brk->paddle_x + BRK_PADDLE_W / 2 - BRK_BALL_SIZE / 2);
      brk->ball_y = BS(BRK_CANVAS_H - BRK_PADDLE_H - BRK_BALL_SIZE - 1);
      brk->ball_dx = 0;
      brk->ball_dy = 0;
    } else {
      brk->mode = BRK_GAME_OVER;
    }
  }
}

void breakout_game_over_tick(Breakout *brk) {
  // Check for trigger press to restart
  bool trigger_now = (brk->joystick->state & JOYSTICK_STATE_TRIGGER) != 0;
  if (trigger_now && !brk->last_trigger_state) {
    breakout_reset_game(brk);
  }
  brk->last_trigger_state = trigger_now;
}

void breakout_move_paddle(Breakout *brk) {
  // Use joystick for movement (x_pos ranges from -100 to +100)
  if (brk->joystick->x_pos < -20) {
    brk->paddle_x = r_max(brk->paddle_x - PADDLE_SPEED, 0);
  } else if (brk->joystick->x_pos > 20) {
    brk->paddle_x = r_min(brk->paddle_x + PADDLE_SPEED,
                          BRK_CANVAS_W - BRK_PADDLE_W);
  }
}

void breakout_launch_ball(Breakout *brk) {
  brk->mode = BRK_PLAYING;
  // Launch at slight angle based on randomness
  int angle_offset = (deadbeef_rand() % 3) - 1; // -1, 0, or 1
  brk->ball_dx = BS(BALL_SPEED_X + angle_offset);
  brk->ball_dy = -BS(BALL_SPEED_Y);
}

void breakout_update_ball(Breakout *brk) {
  // Update ball position
  brk->ball_x += brk->ball_dx;
  brk->ball_y += brk->ball_dy;

  int8_t bx = brk->ball_x >> BRK_SCALE2;
  int8_t by = brk->ball_y >> BRK_SCALE2;

  // Check wall collisions (left/right)
  if (bx <= 0) {
    brk->ball_dx = -brk->ball_dx;
    brk->ball_x = 0;
  } else if (bx + BRK_BALL_SIZE >= BRK_CANVAS_W) {
    brk->ball_dx = -brk->ball_dx;
    brk->ball_x = BS(BRK_CANVAS_W - BRK_BALL_SIZE);
  }

  // Check top wall collision
  if (by <= 0) {
    brk->ball_dy = -brk->ball_dy;
    brk->ball_y = 0;
  }

  // Check paddle collision
  int8_t paddle_y = BRK_CANVAS_H - BRK_PADDLE_H;
  if (by + BRK_BALL_SIZE >= paddle_y && by < paddle_y + BRK_PADDLE_H) {
    if (bx + BRK_BALL_SIZE > brk->paddle_x &&
        bx < brk->paddle_x + BRK_PADDLE_W) {
      // Hit paddle
      brk->ball_dy = -brk->ball_dy;
      brk->ball_y = BS(paddle_y - BRK_BALL_SIZE);

      // Adjust angle based on where it hit the paddle
      int8_t hit_pos = bx - brk->paddle_x + BRK_BALL_SIZE / 2;
      int8_t paddle_center = BRK_PADDLE_W / 2;
      int8_t offset = hit_pos - paddle_center;
      brk->ball_dx += BS(offset / 2);

      // Clamp speed
      if (brk->ball_dx > BS(6)) brk->ball_dx = BS(6);
      if (brk->ball_dx < -BS(6)) brk->ball_dx = -BS(6);
    }
  }

  // Check if ball fell off bottom
  if (by >= BRK_CANVAS_H) {
    brk->lives--;
    brk->mode = BRK_DYING;
    breakout_start_explosion(brk, bx + BRK_BALL_SIZE / 2,
                            BRK_CANVAS_H - 2);
    return;
  }

  // Check brick collisions
  breakout_check_brick_collisions(brk);
}

void breakout_check_brick_collisions(Breakout *brk) {
  int8_t bx = brk->ball_x >> BRK_SCALE2;
  int8_t by = brk->ball_y >> BRK_SCALE2;

  // Calculate which brick row/col the ball might be hitting
  for (int row = 0; row < BRK_BRICK_ROWS; row++) {
    for (int col = 0; col < BRK_BRICK_COLS; col++) {
      int idx = row * BRK_BRICK_COLS + col;
      if (!brk->bricks[idx]) {
        continue;
      }

      int8_t brick_x = col * (BRK_BRICK_W + BRICK_GAP);
      int8_t brick_y = BRICKS_START_Y + row * (BRK_BRICK_H + BRICK_GAP);

      // Check collision
      if (bx + BRK_BALL_SIZE > brick_x && bx < brick_x + BRK_BRICK_W &&
          by + BRK_BALL_SIZE > brick_y && by < brick_y + BRK_BRICK_H) {
        // Hit brick!
        brk->bricks[idx] = FALSE;
        brk->bricks_remaining--;
        brk->score += BRICK_POINTS;

        // Determine bounce direction based on which side was hit
        int8_t ball_center_x = bx + BRK_BALL_SIZE / 2;
        int8_t ball_center_y = by + BRK_BALL_SIZE / 2;
        int8_t brick_center_x = brick_x + BRK_BRICK_W / 2;
        int8_t brick_center_y = brick_y + BRK_BRICK_H / 2;

        int8_t dx = ball_center_x - brick_center_x;
        int8_t dy = ball_center_y - brick_center_y;

        if (abs(dx) > abs(dy)) {
          // Hit from side
          brk->ball_dx = -brk->ball_dx;
        } else {
          // Hit from top/bottom
          brk->ball_dy = -brk->ball_dy;
        }

        // Check for victory
        if (brk->bricks_remaining == 0) {
          brk->mode = BRK_VICTORY;
        }

        return; // Only destroy one brick per frame
      }
    }
  }
}

void breakout_start_explosion(Breakout *brk, int8_t x, int8_t y) {
  brk->explosion_x = x;
  brk->explosion_y = y;
  brk->explosion_radius = 1;
}

void breakout_paint_once(Breakout *brk) {
  if (!brk->focused) {
    return;
  }

  Screen4 *s4 = brk->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);

  if (brk->mode == BRK_WAITING || brk->mode == BRK_PLAYING ||
      brk->mode == BRK_DYING) {
    breakout_paint_bricks(brk, rrect);
    breakout_paint_paddle(brk, rrect);
    if (brk->mode != BRK_DYING) {
      breakout_paint_ball(brk, rrect);
    }
  } else if (brk->mode == BRK_GAME_OVER || brk->mode == BRK_VICTORY) {
    // Show final brick state
    breakout_paint_bricks(brk, rrect);
  }

  if (brk->explosion_radius > 0) {
    breakout_paint_explosion(brk, rrect);
  }

  raster_draw_buffers(rrect);

  // Paint lives
  {
    char buf[16];
    int_to_string2(buf, NUM_DIGITS, 1, brk->lives);
    ascii_to_bitmap_str(brk->lives_bbuf.buffer, NUM_DIGITS, buf);
    board_buffer_draw(&brk->lives_bbuf);
  }

  // Paint score
  {
    char buf[16];
    int_to_string2(buf, NUM_DIGITS, 1, brk->score);
    ascii_to_bitmap_str(brk->score_bbuf.buffer, NUM_DIGITS, buf);
    board_buffer_draw(&brk->score_bbuf);
  }
}

void breakout_paint_paddle(Breakout *brk, RectRegion *rrect) {
  int8_t y = BRK_CANVAS_H - BRK_PADDLE_H;
  for (int dx = 0; dx < BRK_PADDLE_W; dx++) {
    for (int dy = 0; dy < BRK_PADDLE_H; dy++) {
      raster_paint_pixel(rrect, brk->paddle_x + dx, y + dy);
    }
  }
}

void breakout_paint_ball(Breakout *brk, RectRegion *rrect) {
  int8_t bx = brk->ball_x >> BRK_SCALE2;
  int8_t by = brk->ball_y >> BRK_SCALE2;
  for (int dx = 0; dx < BRK_BALL_SIZE; dx++) {
    for (int dy = 0; dy < BRK_BALL_SIZE; dy++) {
      raster_paint_pixel(rrect, bx + dx, by + dy);
    }
  }
}

void breakout_paint_bricks(Breakout *brk, RectRegion *rrect) {
  for (int row = 0; row < BRK_BRICK_ROWS; row++) {
    for (int col = 0; col < BRK_BRICK_COLS; col++) {
      int idx = row * BRK_BRICK_COLS + col;
      if (!brk->bricks[idx]) {
        continue;
      }

      int8_t x = col * (BRK_BRICK_W + BRICK_GAP);
      int8_t y = BRICKS_START_Y + row * (BRK_BRICK_H + BRICK_GAP);

      for (int dx = 0; dx < BRK_BRICK_W; dx++) {
        for (int dy = 0; dy < BRK_BRICK_H; dy++) {
          raster_paint_pixel(rrect, x + dx, y + dy);
        }
      }
    }
  }
}

void breakout_paint_explosion(Breakout *brk, RectRegion *rrect) {
  int8_t radius = brk->explosion_radius;
  int8_t cx = brk->explosion_x;
  int8_t cy = brk->explosion_y;

  // Simple expanding circle
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        raster_paint_pixel(rrect, cx + dx, cy + dy);
      }
    }
  }
}

UIEventDisposition breakout_event_handler(UIEventHandler *raw_handler,
                                          UIEvent evt) {
  Breakout *brk = ((BreakoutHandler *)raw_handler)->breakout;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case uie_focus:
      if (!brk->focused) {
        s4_show(brk->s4);
        board_buffer_push(&brk->lives_bbuf, brk->lives_boardnum);
        board_buffer_push(&brk->score_bbuf, brk->score_boardnum);
      }
      brk->focused = TRUE;
      break;
    case uie_escape:
      if (brk->focused) {
        s4_hide(brk->s4);
        board_buffer_pop(&brk->lives_bbuf);
        board_buffer_pop(&brk->score_bbuf);
      }
      brk->focused = FALSE;
      result = uied_blur;
      break;
  }
  breakout_paint_once(brk);
  return result;
}
