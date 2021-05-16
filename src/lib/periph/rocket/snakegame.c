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

#include "periph/rocket/snakegame.h"

#include "core/rulos.h"
#include "periph/audio/sound.h"
#include "periph/rasters/rasters.h"

#define SNAKE_FREQ 32  // Animation frequency, Hz.
#define MOVE_RATE  3   // Ticks per moves animation
#define GROW_RATE  24  // Ticks per snake growth

// TODO: add snake-advance and snake-turn and snake explosion sounds?
// TODO: S6? :v)
// TODO: speed up pace as it gets longer?
// TODO: Blink GameOver message?

void snake_update(Snake *snake);
UIEventDisposition snake_event_handler(UIEventHandler *raw_handler,
                                       UIEvent evt);
void snake_paint_once(Snake *snake);
void snake_init_map(Map *map);
void snake_reset_game(Snake *snake);
Direction get_cell(Map *map, uint8_t x, uint8_t y);
void snake_advance_head(Snake *snake);
void snake_advance_tail(Snake *snake);
void snake_playing_tick(Snake *snake);
void snake_exploding_tick(Snake *snake);
void snake_game_over_tick(Snake *snake);
void populate_sqrt_table();

void snake_init(Snake *snake, Screen4 *s4, AudioClient *audioClient,
                uint8_t score_boardnum, uint8_t status_boardnum) {
  snake->s4 = s4;
  snake->handler.func = (UIEventHandlerFunc)snake_event_handler;
  snake->handler.snake = snake;
  snake->audioClient = audioClient;
  snake->score_boardnum = score_boardnum;
  board_buffer_init(&snake->score_bbuf DBG_BBUF_LABEL("snake score"));
  snake->status_boardnum = status_boardnum;
  board_buffer_init(&snake->status_bbuf DBG_BBUF_LABEL("snake status"));
  snake->focused = FALSE;
  snake_reset_game(snake);
  populate_sqrt_table();

  schedule_us(1, (ActivationFuncPtr)snake_update, snake);
}

void snake_reset_game(Snake *snake) {
  // LOG("playing snake!");
  snake_init_map(&snake->map);
  snake->head.x = CANVAS_W / 4;
  snake->head.y = CANVAS_H / 2;
  snake->tail = snake->head;
  snake->direction = RIGHT;
  snake->move_clock = MOVE_RATE;
  snake->grow_clock = GROW_RATE;
  snake->goal_length = 4;
  snake->length = 0;
  snake->explosion_radius = 0;
  snake->mode = PLAYING;
}

void snake_init_map(Map *map) {
  for (uint8_t y = 0; y < CANVAS_H; y++) {
    for (uint8_t x = 0; x < CANVAS_W; x++) {
      map->cell[y][x] = EMPTY;
    }
  }
}

bool in_bounds(uint8_t x, uint8_t y) {
  return (x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H);
}

static inline Point add(Point a, Direction d) {
  Point r = a;
  switch (d) {
    case RIGHT:
      r.x += 1;
      break;
    case UP:
      r.y -= 1;
      break;
    case LEFT:
      r.x -= 1;
      break;
    case DOWN:
      r.y += 1;
      break;
    default:
      assert(false);
  }
  return r;
}

static inline bool occupied(Map *map, Point a) {
  return get_cell(map, a.x, a.y) != EMPTY ||
         get_cell(map, a.x - 1, a.y) == RIGHT ||
         get_cell(map, a.x + 1, a.y) == LEFT ||
         get_cell(map, a.x, a.y - 1) == DOWN ||
         get_cell(map, a.x, a.y + 1) == UP;
}

#if 0
static inline bool get_game_over(Snake *snake) {
  bool rc = snake->direction == EMPTY;
  if (rc) {
    LOG("snake: game over");
  }
  return rc;
}
#endif

static inline void explode(Snake *snake) {
  snake->mode = EXPLODING;
}

void snake_tick(Snake *snake) {
  switch (snake->mode) {
    case PLAYING:
      snake_playing_tick(snake);
      break;
    case EXPLODING:
      snake_exploding_tick(snake);
      break;
    case GAME_OVER:
      snake_game_over_tick(snake);
      break;
  }
}

void snake_playing_tick(Snake *snake) {
  snake->grow_clock--;
  if (snake->grow_clock == 0) {
    snake->grow_clock = GROW_RATE;
    snake->goal_length++;
  }

  snake->move_clock--;
  if (snake->move_clock == 0) {
    snake->move_clock = MOVE_RATE;
    snake_advance_head(snake);
    if (snake->length > snake->goal_length) {
      snake_advance_tail(snake);
    }
  }
  // LOG("grow clock %d length %d goal %d", snake->grow_clock, snake->length,
  // snake->goal_length);
}

void snake_exploding_tick(Snake *snake) {
  snake->explosion_radius++;
  if (snake->explosion_radius > CANVAS_W) {
    snake->explosion_radius = 0;
    snake->mode = GAME_OVER;
  }
  LOG("Exploding; %d", snake->explosion_radius);
}

void snake_game_over_tick(Snake *snake) {
  snake->explosion_radius = 0;
}

void snake_advance_head(Snake *snake) {
  Point head = snake->head;
  Point next_head = add(head, snake->direction);
  if (!in_bounds(next_head.x, next_head.y)) {
    // LOG("wall collision");
    explode(snake);
    return;
  }
  if (occupied(&snake->map, next_head)) {
    // LOG("self collision");
    explode(snake);
    return;
  }
  assert(in_bounds(head.x, head.y));
  snake->map.cell[head.y][head.x] = snake->direction;
  snake->head = next_head;
  snake->length++;
  // LOG("grow");
}

void snake_advance_tail(Snake *snake) {
  Point tail = snake->tail;
  Direction tdir = get_cell(&snake->map, tail.x, tail.y);
  if (tdir == EMPTY) {
    // zero-length snake
    return;
  }
  snake->map.cell[tail.y][tail.x] = EMPTY;
  snake->tail = add(tail, tdir);
  snake->length--;
  // LOG("shrink");
}

void snake_update(Snake *snake) {
  schedule_us(1000000 / SNAKE_FREQ, (ActivationFuncPtr)snake_update, snake);
  if (snake->focused) {
    snake_tick(snake);
  }
  snake_paint_once(snake);
}

// Returns the direction the snake leaves cell. Out-of-bounds x,y return EMPTY.
Direction get_cell(Map *map, uint8_t x, uint8_t y) {
  if (!in_bounds(x, y)) {
    return EMPTY;
  }
  return map->cell[y][x];
}

#define MAX_SQRT (CANVAS_W * CANVAS_W)
int sqrt_table[MAX_SQRT];

void populate_sqrt_table() {
  for (int i = 0; i < MAX_SQRT; i++) {
    sqrt_table[i] = isqrt(i);
  }
}

void snake_paint_once(Snake *snake) {
  if (!snake->focused) {
    // don't paint on borrowed s4 surface
    return;
  }

  Screen4 *s4 = snake->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);
  Map *map = &snake->map;

  // paint horizontal segments
  for (uint8_t y = 0; y < CANVAS_H; y++) {
    for (uint8_t x = 0; x < CANVAS_W; x++) {
      uint8_t py = 2 * y;
      uint8_t px = 2 * x;
      raster_paint_pixel_v(
          rrect, px + 1, py,
          get_cell(map, x, y) == RIGHT || get_cell(map, x + 1, y) == LEFT);
      raster_paint_pixel_v(
          rrect, px, py + 1,
          get_cell(map, x, y) == DOWN || get_cell(map, x, y + 1) == UP);
    }
  }

  // Paint explosion over the top
  uint8_t radius = snake->explosion_radius;
  Point head = snake->head;
  head.x *= 2;
  head.y *= 2;
  int radius2 = radius * radius;
  // LOG("center %d, %d", head.x, head.y);
  if (radius > 0) {
    for (int dy = -radius; dy <= radius; dy++) {
      int y = head.y + dy;
      int dx2 = radius2 - dy * dy;
      int dx = 0;
      if (dx2 >= 0 && dx2 < MAX_SQRT) {
        dx = sqrt_table[dx2];
      }
      int xlim = head.x + dx;
      for (int x = head.x - dx; x <= xlim; x++) {
        raster_paint_pixel(rrect, x, y);
      }
    }
  }

  raster_draw_buffers(rrect);

  // Paint the score.
  {
    char buf[16], *p = buf;
    p += int_to_string2(p, NUM_DIGITS, 1, snake->length);
    ascii_to_bitmap_str(snake->score_bbuf.buffer, NUM_DIGITS, buf);
    board_buffer_draw(&snake->score_bbuf);
  }
  switch (snake->mode) {
    case PLAYING:
      ascii_to_bitmap_str(snake->status_bbuf.buffer, NUM_DIGITS, "Slither.");
      break;
    case EXPLODING:
      ascii_to_bitmap_str(snake->status_bbuf.buffer, NUM_DIGITS, " Ouch!  ");
      break;
    case GAME_OVER:
      ascii_to_bitmap_str(snake->status_bbuf.buffer, NUM_DIGITS, "GameOver");
      break;
  }
  board_buffer_draw(&snake->status_bbuf);
}

void snake_turn(Snake *snake, int8_t dir) {
  switch (snake->mode) {
    case PLAYING:
      snake->direction = (snake->direction + dir + 4) % 4;
      break;
    case EXPLODING:
      break;
    case GAME_OVER:
      snake_reset_game(snake);
      break;
  }
}

UIEventDisposition snake_event_handler(UIEventHandler *raw_handler,
                                       UIEvent evt) {
  Snake *snake = ((SnakeHandler *)raw_handler)->snake;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case '1':
    case 'p':
    case 'a':
    case 'e':
      snake_turn(snake, -1);
      break;
    case '4':
    case 'q':
    case 'b':
    case 'f':
      snake_turn(snake, 1);
      break;
    case uie_focus:
      if (!snake->focused) {
        s4_show(snake->s4);
        board_buffer_push(&snake->score_bbuf, snake->score_boardnum);
        board_buffer_push(&snake->status_bbuf, snake->status_boardnum);
      }
      snake->focused = TRUE;
      break;
    case uie_escape:
      if (snake->focused) {
        s4_hide(snake->s4);
        board_buffer_pop(&snake->score_bbuf);
        board_buffer_pop(&snake->status_bbuf);
      }
      snake->focused = FALSE;
      result = uied_blur;
      break;
  }
  snake_paint_once(snake);
  return result;
}
