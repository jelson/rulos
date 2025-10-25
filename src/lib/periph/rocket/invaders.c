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

#include "periph/rocket/invaders.h"

#include "core/rulos.h"
#include "periph/audio/sound.h"
#include "periph/rasters/rasters.h"

#define INV_FREQ         32  // Animation frequency, Hz
#define ALIEN_MOVE_RATE  16  // Ticks per alien formation move
#define ALIEN_SHOOT_RATE 20  // Ticks per alien shoot attempt
#define BULLET_SPEED     1   // Pixels per tick
#define SHIP_SPEED       2   // Pixels per joystick move
#define ALIEN_DROP       2   // How far aliens drop when changing direction
#define ALIEN_POINTS     10  // Points per alien killed

void invaders_update(Invaders *inv);
UIEventDisposition invaders_event_handler(UIEventHandler *raw_handler,
                                          UIEvent evt);
void invaders_paint_once(Invaders *inv);
void invaders_reset_game(Invaders *inv);
void invaders_init_aliens(Invaders *inv);
void invaders_tick(Invaders *inv);
void invaders_playing_tick(Invaders *inv);
void invaders_exploding_tick(Invaders *inv);
void invaders_game_over_tick(Invaders *inv);
void invaders_move_ship(Invaders *inv);
void invaders_fire_player_bullet(Invaders *inv);
void invaders_update_bullets(Invaders *inv);
void invaders_update_aliens(Invaders *inv);
void invaders_check_collisions(Invaders *inv);
void invaders_paint_ship(Invaders *inv, RectRegion *rrect);
void invaders_paint_aliens(Invaders *inv, RectRegion *rrect);
void invaders_paint_bullets(Invaders *inv, RectRegion *rrect);
void invaders_paint_explosion(Invaders *inv, RectRegion *rrect);
void invaders_start_explosion(Invaders *inv, int8_t x, int8_t y);

void invaders_init(Invaders *inv, Screen4 *s4, AudioClient *audioClient,
                   JoystickState_t *joystick, uint8_t lives_boardnum,
                   uint8_t score_boardnum) {
  inv->s4 = s4;
  inv->handler.func = (UIEventHandlerFunc)invaders_event_handler;
  inv->handler.invaders = inv;
  inv->audioClient = audioClient;
  inv->joystick = joystick;
  inv->lives_boardnum = lives_boardnum;
  board_buffer_init(&inv->lives_bbuf DBG_BBUF_LABEL("invaders lives"));
  inv->score_boardnum = score_boardnum;
  board_buffer_init(&inv->score_bbuf DBG_BBUF_LABEL("invaders score"));
  inv->focused = FALSE;
  invaders_reset_game(inv);

  schedule_us(1, (ActivationFuncPtr)invaders_update, inv);
}

void invaders_reset_game(Invaders *inv) {
  inv->mode = INV_PLAYING;
  inv->ship_x = INV_CANVAS_W / 2 - INV_SHIP_W / 2;
  inv->score = 0;
  inv->lives = 3;
  inv->explosion_radius = 0;
  inv->last_trigger_state = FALSE;

  // Clear all bullets
  for (int i = 0; i < INV_MAX_PLAYER_BULLETS; i++) {
    inv->player_bullets[i].active = FALSE;
  }
  for (int i = 0; i < INV_MAX_ALIEN_BULLETS; i++) {
    inv->alien_bullets[i].active = FALSE;
  }

  invaders_init_aliens(inv);
}

void invaders_init_aliens(Invaders *inv) {
  inv->aliens.x = 2;
  inv->aliens.y = 2;
  inv->aliens.dx = 1;
  inv->aliens.move_clock = ALIEN_MOVE_RATE;
  inv->aliens.shoot_clock = ALIEN_SHOOT_RATE;
  inv->aliens_remaining = INV_MAX_ALIENS;

  for (int i = 0; i < INV_MAX_ALIENS; i++) {
    inv->aliens.alive[i] = TRUE;
  }
}

void invaders_update(Invaders *inv) {
  schedule_us(1000000 / INV_FREQ, (ActivationFuncPtr)invaders_update, inv);
  if (inv->focused) {
    invaders_tick(inv);
  }
  invaders_paint_once(inv);
}

void invaders_tick(Invaders *inv) {
  switch (inv->mode) {
    case INV_PLAYING:
      invaders_playing_tick(inv);
      break;
    case INV_PLAYER_EXPLODING:
      invaders_exploding_tick(inv);
      break;
    case INV_GAME_OVER:
    case INV_VICTORY:
      invaders_game_over_tick(inv);
      break;
  }
}

void invaders_playing_tick(Invaders *inv) {
  invaders_move_ship(inv);

  // Check for trigger press (fire on rising edge)
  bool trigger_now = (inv->joystick->state & JOYSTICK_STATE_TRIGGER) != 0;
  if (trigger_now && !inv->last_trigger_state) {
    invaders_fire_player_bullet(inv);
  }
  inv->last_trigger_state = trigger_now;

  invaders_update_bullets(inv);
  invaders_update_aliens(inv);
  invaders_check_collisions(inv);
}

void invaders_exploding_tick(Invaders *inv) {
  // Continue updating bullets and aliens during explosion for visual continuity
  invaders_update_bullets(inv);
  invaders_update_aliens(inv);

  inv->explosion_radius++;
  if (inv->explosion_radius > 8) {
    inv->explosion_radius = 0;
    if (inv->lives > 0) {
      inv->mode = INV_PLAYING;
      // Clear alien bullets and player bullets for fresh start
      for (int i = 0; i < INV_MAX_ALIEN_BULLETS; i++) {
        inv->alien_bullets[i].active = FALSE;
      }
      for (int i = 0; i < INV_MAX_PLAYER_BULLETS; i++) {
        inv->player_bullets[i].active = FALSE;
      }
    } else {
      inv->mode = INV_GAME_OVER;
    }
  }
}

void invaders_game_over_tick(Invaders *inv) {
  // Check for trigger press to restart (on rising edge)
  bool trigger_now = (inv->joystick->state & JOYSTICK_STATE_TRIGGER) != 0;
  if (trigger_now && !inv->last_trigger_state) {
    invaders_reset_game(inv);
  }
  inv->last_trigger_state = trigger_now;
}

void invaders_move_ship(Invaders *inv) {
  // Use joystick for movement (x_pos ranges from -100 to +100)
  if (inv->joystick->x_pos < -20) {
    inv->ship_x = r_max(inv->ship_x - SHIP_SPEED, 0);
  } else if (inv->joystick->x_pos > 20) {
    inv->ship_x = r_min(inv->ship_x + SHIP_SPEED, INV_CANVAS_W - INV_SHIP_W);
  }
}

void invaders_fire_player_bullet(Invaders *inv) {
  // Find an inactive bullet slot
  for (int i = 0; i < INV_MAX_PLAYER_BULLETS; i++) {
    if (!inv->player_bullets[i].active) {
      inv->player_bullets[i].active = TRUE;
      inv->player_bullets[i].x = inv->ship_x + INV_SHIP_W / 2;
      inv->player_bullets[i].y = INV_CANVAS_H - INV_SHIP_H - 2;
      // TODO: add shoot sound
      break;
    }
  }
}

void invaders_update_bullets(Invaders *inv) {
  // Update player bullets
  for (int i = 0; i < INV_MAX_PLAYER_BULLETS; i++) {
    if (inv->player_bullets[i].active) {
      inv->player_bullets[i].y -= BULLET_SPEED;
      if (inv->player_bullets[i].y < 0) {
        inv->player_bullets[i].active = FALSE;
      }
    }
  }

  // Update alien bullets
  for (int i = 0; i < INV_MAX_ALIEN_BULLETS; i++) {
    if (inv->alien_bullets[i].active) {
      inv->alien_bullets[i].y += BULLET_SPEED;
      if (inv->alien_bullets[i].y >= INV_CANVAS_H) {
        inv->alien_bullets[i].active = FALSE;
      }
    }
  }
}

void invaders_update_aliens(Invaders *inv) {
  // Move aliens
  inv->aliens.move_clock--;
  if (inv->aliens.move_clock == 0) {
    inv->aliens.move_clock = ALIEN_MOVE_RATE;

    // Calculate bounds of alien formation
    int8_t leftmost = INV_ALIEN_COLS;
    int8_t rightmost = -1;
    for (int row = 0; row < INV_ALIEN_ROWS; row++) {
      for (int col = 0; col < INV_ALIEN_COLS; col++) {
        int idx = row * INV_ALIEN_COLS + col;
        if (inv->aliens.alive[idx]) {
          if (col < leftmost)
            leftmost = col;
          if (col > rightmost)
            rightmost = col;
        }
      }
    }

    int8_t formation_left = inv->aliens.x + leftmost * (INV_ALIEN_W + 1);
    int8_t formation_right =
        inv->aliens.x + rightmost * (INV_ALIEN_W + 1) + INV_ALIEN_W;

    // Check if we need to change direction
    bool change_dir = FALSE;
    if (inv->aliens.dx > 0 && formation_right >= INV_CANVAS_W - 1) {
      change_dir = TRUE;
    } else if (inv->aliens.dx < 0 && formation_left <= 0) {
      change_dir = TRUE;
    }

    if (change_dir) {
      inv->aliens.dx = -inv->aliens.dx;
      inv->aliens.y += ALIEN_DROP;

      // Check if aliens reached the bottom (game over)
      int8_t lowest_row = -1;
      for (int row = INV_ALIEN_ROWS - 1; row >= 0; row--) {
        for (int col = 0; col < INV_ALIEN_COLS; col++) {
          int idx = row * INV_ALIEN_COLS + col;
          if (inv->aliens.alive[idx]) {
            lowest_row = row;
            goto found_lowest;
          }
        }
      }
    found_lowest:
      if (lowest_row >= 0) {
        int8_t lowest_y =
            inv->aliens.y + lowest_row * (INV_ALIEN_H + 1) + INV_ALIEN_H;
        if (lowest_y >= INV_CANVAS_H - INV_SHIP_H - 1) {
          inv->mode = INV_GAME_OVER;
        }
      }
    } else {
      inv->aliens.x += inv->aliens.dx;
    }
  }

  // Aliens shoot
  inv->aliens.shoot_clock--;
  if (inv->aliens.shoot_clock == 0) {
    inv->aliens.shoot_clock = ALIEN_SHOOT_RATE;

    // Pick a random alive alien to shoot
    if (inv->aliens_remaining > 0) {
      int shooter_idx = deadbeef_rand() % inv->aliens_remaining;
      int count = 0;
      for (int i = 0; i < INV_MAX_ALIENS; i++) {
        if (inv->aliens.alive[i]) {
          if (count == shooter_idx) {
            // This alien shoots
            int row = i / INV_ALIEN_COLS;
            int col = i % INV_ALIEN_COLS;
            int8_t alien_x = inv->aliens.x + col * (INV_ALIEN_W + 1);
            int8_t alien_y = inv->aliens.y + row * (INV_ALIEN_H + 1);

            // Find inactive bullet slot
            for (int j = 0; j < INV_MAX_ALIEN_BULLETS; j++) {
              if (!inv->alien_bullets[j].active) {
                inv->alien_bullets[j].active = TRUE;
                inv->alien_bullets[j].x = alien_x + INV_ALIEN_W / 2;
                inv->alien_bullets[j].y = alien_y + INV_ALIEN_H;
                break;
              }
            }
            break;
          }
          count++;
        }
      }
    }
  }
}

void invaders_check_collisions(Invaders *inv) {
  // Check player bullets hitting aliens
  for (int b = 0; b < INV_MAX_PLAYER_BULLETS; b++) {
    if (!inv->player_bullets[b].active)
      continue;

    int8_t bx = inv->player_bullets[b].x;
    int8_t by = inv->player_bullets[b].y;

    for (int row = 0; row < INV_ALIEN_ROWS; row++) {
      for (int col = 0; col < INV_ALIEN_COLS; col++) {
        int idx = row * INV_ALIEN_COLS + col;
        if (!inv->aliens.alive[idx])
          continue;

        int8_t alien_x = inv->aliens.x + col * (INV_ALIEN_W + 1);
        int8_t alien_y = inv->aliens.y + row * (INV_ALIEN_H + 1);

        if (bx >= alien_x && bx < alien_x + INV_ALIEN_W && by >= alien_y &&
            by < alien_y + INV_ALIEN_H) {
          // Hit!
          inv->aliens.alive[idx] = FALSE;
          inv->aliens_remaining--;
          inv->player_bullets[b].active = FALSE;
          inv->score += ALIEN_POINTS;

          invaders_start_explosion(inv, alien_x + INV_ALIEN_W / 2,
                                    alien_y + INV_ALIEN_H / 2);

          if (inv->aliens_remaining == 0) {
            inv->mode = INV_VICTORY;
          }
          goto next_bullet;
        }
      }
    }
  next_bullet:;
  }

  // Check alien bullets hitting player
  int8_t ship_y = INV_CANVAS_H - INV_SHIP_H;
  for (int b = 0; b < INV_MAX_ALIEN_BULLETS; b++) {
    if (!inv->alien_bullets[b].active)
      continue;

    int8_t bx = inv->alien_bullets[b].x;
    int8_t by = inv->alien_bullets[b].y;

    if (bx >= inv->ship_x && bx < inv->ship_x + INV_SHIP_W && by >= ship_y &&
        by < ship_y + INV_SHIP_H) {
      // Player hit!
      inv->alien_bullets[b].active = FALSE;
      inv->lives--;
      inv->mode = INV_PLAYER_EXPLODING;
      invaders_start_explosion(inv, inv->ship_x + INV_SHIP_W / 2,
                                ship_y + INV_SHIP_H / 2);
      break;
    }
  }
}

void invaders_start_explosion(Invaders *inv, int8_t x, int8_t y) {
  inv->explosion_x = x;
  inv->explosion_y = y;
  inv->explosion_radius = 1;
}

void invaders_paint_once(Invaders *inv) {
  if (!inv->focused) {
    return;
  }

  Screen4 *s4 = inv->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);

  if (inv->mode == INV_PLAYING || inv->mode == INV_PLAYER_EXPLODING) {
    invaders_paint_aliens(inv, rrect);
    invaders_paint_bullets(inv, rrect);
    if (inv->mode == INV_PLAYING) {
      invaders_paint_ship(inv, rrect);
    }
  } else if (inv->mode == INV_GAME_OVER || inv->mode == INV_VICTORY) {
    // Display the final state (aliens that remain, no bullets, no ship)
    invaders_paint_aliens(inv, rrect);
  }

  if (inv->explosion_radius > 0) {
    invaders_paint_explosion(inv, rrect);
  }

  raster_draw_buffers(rrect);

  // Paint lives
  {
    char buf[16];
    int_to_string2(buf, NUM_DIGITS, 1, inv->lives);
    ascii_to_bitmap_str(inv->lives_bbuf.buffer, NUM_DIGITS, buf);
    board_buffer_draw(&inv->lives_bbuf);
  }

  // Paint score
  {
    char buf[16];
    int_to_string2(buf, NUM_DIGITS, 1, inv->score);
    ascii_to_bitmap_str(inv->score_bbuf.buffer, NUM_DIGITS, buf);
    board_buffer_draw(&inv->score_bbuf);
  }
}

void invaders_paint_ship(Invaders *inv, RectRegion *rrect) {
  int8_t y = INV_CANVAS_H - INV_SHIP_H;
  // Draw a simple ship shape
  for (int dx = 0; dx < INV_SHIP_W; dx++) {
    for (int dy = 0; dy < INV_SHIP_H; dy++) {
      raster_paint_pixel(rrect, inv->ship_x + dx, y + dy);
    }
  }
}

void invaders_paint_aliens(Invaders *inv, RectRegion *rrect) {
  for (int row = 0; row < INV_ALIEN_ROWS; row++) {
    for (int col = 0; col < INV_ALIEN_COLS; col++) {
      int idx = row * INV_ALIEN_COLS + col;
      if (!inv->aliens.alive[idx])
        continue;

      int8_t x = inv->aliens.x + col * (INV_ALIEN_W + 1);
      int8_t y = inv->aliens.y + row * (INV_ALIEN_H + 1);

      for (int dx = 0; dx < INV_ALIEN_W; dx++) {
        for (int dy = 0; dy < INV_ALIEN_H; dy++) {
          raster_paint_pixel(rrect, x + dx, y + dy);
        }
      }
    }
  }
}

void invaders_paint_bullets(Invaders *inv, RectRegion *rrect) {
  // Paint player bullets
  for (int i = 0; i < INV_MAX_PLAYER_BULLETS; i++) {
    if (inv->player_bullets[i].active) {
      raster_paint_pixel(rrect, inv->player_bullets[i].x,
                         inv->player_bullets[i].y);
      raster_paint_pixel(rrect, inv->player_bullets[i].x,
                         inv->player_bullets[i].y + 1);
    }
  }

  // Paint alien bullets
  for (int i = 0; i < INV_MAX_ALIEN_BULLETS; i++) {
    if (inv->alien_bullets[i].active) {
      raster_paint_pixel(rrect, inv->alien_bullets[i].x,
                         inv->alien_bullets[i].y);
      raster_paint_pixel(rrect, inv->alien_bullets[i].x,
                         inv->alien_bullets[i].y + 1);
    }
  }
}

void invaders_paint_explosion(Invaders *inv, RectRegion *rrect) {
  int8_t radius = inv->explosion_radius;
  int8_t cx = inv->explosion_x;
  int8_t cy = inv->explosion_y;

  // Simple expanding circle
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        raster_paint_pixel(rrect, cx + dx, cy + dy);
      }
    }
  }
}

UIEventDisposition invaders_event_handler(UIEventHandler *raw_handler,
                                          UIEvent evt) {
  Invaders *inv = ((InvadersHandler *)raw_handler)->invaders;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case ' ':  // Space bar for shooting
    case 'j':  // Joystick trigger
      if (inv->mode == INV_PLAYING) {
        invaders_fire_player_bullet(inv);
      } else if (inv->mode == INV_GAME_OVER || inv->mode == INV_VICTORY) {
        invaders_reset_game(inv);
      }
      break;
    case uie_focus:
      if (!inv->focused) {
        s4_show(inv->s4);
        board_buffer_push(&inv->lives_bbuf, inv->lives_boardnum);
        board_buffer_push(&inv->score_bbuf, inv->score_boardnum);
      }
      inv->focused = TRUE;
      break;
    case uie_escape:
      if (inv->focused) {
        s4_hide(inv->s4);
        board_buffer_pop(&inv->lives_bbuf);
        board_buffer_pop(&inv->score_bbuf);
      }
      inv->focused = FALSE;
      result = uied_blur;
      break;
  }
  invaders_paint_once(inv);
  return result;
}
