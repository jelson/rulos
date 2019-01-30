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

#include "../app/snaketest/snakegame.h" // TODO move into lib
#include "core/rulos.h"
#include "periph/rasters/rasters.h"
#include "periph/rocket/sound.h"

#define SNAKE_FREQ 8

void snake_update(Snake *snake);
UIEventDisposition snake_event_handler(UIEventHandler *raw_handler, UIEvent evt);
void snake_paint_once(Snake *snake);
void snake_init_map(Map* map);
void snake_reset_game(Snake *snake);
Direction get_cell(Map* map, uint8_t x, uint8_t y);
void snake_advance_head(Snake *snake);
void snake_advance_tail(Snake *snake);

void snake_init(Snake *snake, Screen4 *s4, AudioClient *audioClient, uint8_t score_boardnum) {
  snake->s4 = s4;
  snake->handler.func = (UIEventHandlerFunc)snake_event_handler;
  snake->handler.snake = snake;
  snake->audioClient = audioClient;
  snake->score_boardnum = score_boardnum;
  board_buffer_init(&snake->score_bbuf DBG_BBUF_LABEL("snake score"));
  snake->focused = FALSE;
  snake_reset_game(snake);

  schedule_us(1, (ActivationFuncPtr)snake_update, snake);
}

void snake_reset_game(Snake *snake) {
  snake_init_map(&snake->map);
  snake->head.x = CANVAS_W/4;
  snake->head.y = CANVAS_H/2;
  snake->tail = snake->head;
  snake->direction = RIGHT;
  snake->ticks_per_grow = 6;
  snake->grow_clock = snake->ticks_per_grow;
  snake->goal_length = 4;
  snake->length = 0;
}

void snake_init_map(Map* map) {
  for (uint8_t y=0; y<CANVAS_H; y++) {
    for (uint8_t x=0; x<CANVAS_W; x++) {
      map->cell[y][x] = EMPTY;
    }
  }
}

bool in_bounds(uint8_t x, uint8_t y) {
  return (x>=0 && x<CANVAS_W && y>=0 && y<CANVAS_H);
}

inline Point add(Point a, Direction d) {
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

inline bool occupied(Map* map, Point a) {
  return get_cell(map, a.x, a.y) != EMPTY
    || get_cell(map, a.x-1, a.y)==RIGHT || get_cell(map, a.x+1, a.y) == LEFT
    || get_cell(map, a.x, a.y-1)==DOWN || get_cell(map, a.x, a.y+1) == UP;
}

inline bool get_game_over(Snake* snake) {
  bool rc = snake->direction == EMPTY;
  //if (rc) { LOG("game over\n"); }
  return rc;
}

inline void set_game_over(Snake* snake) {
  snake->direction = EMPTY;
  snake_reset_game(snake);  //XXX
}

void snake_tick(Snake *snake) {
  snake->grow_clock--;
  if (snake->grow_clock == 0) {
    snake->grow_clock = snake->ticks_per_grow;
    snake->goal_length++;
  }

  snake_advance_head(snake);
  if (snake->length > snake->goal_length) {
    snake_advance_tail(snake);
  }
  //LOG("grow clock %d length %d goal %d\n", snake->grow_clock, snake->length, snake->goal_length);
}

void snake_advance_head(Snake *snake) {
  if (get_game_over(snake)) {
    return;
  }
  Point head = snake->head;
  Point next_head = add(head, snake->direction);
  if (!in_bounds(next_head.x, next_head.y)) {
    //LOG("wall collision\n");
    set_game_over(snake);
    return;
  }
  if (occupied(&snake->map, next_head)) {
    //LOG("self collision\n");
    set_game_over(snake);
    return;
  }
  assert(in_bounds(head.x, head.y));
  snake->map.cell[head.y][head.x] = snake->direction;
  snake->head = next_head;
  snake->length++;
  //LOG("grow\n");
}

void snake_advance_tail(Snake *snake) {
  if (get_game_over(snake)) {
    return;
  }
  Point tail = snake->tail;
  Direction tdir = get_cell(&snake->map, tail.x, tail.y);
  if (tdir==EMPTY) {
    // zero-length snake
    return;
  }
  snake->map.cell[tail.y][tail.x] = EMPTY;
  snake->tail =add(tail, tdir);
  snake->length--;
  //LOG("shrink\n");
}

void snake_update(Snake *snake) {
  schedule_us(1000000 / SNAKE_FREQ, (ActivationFuncPtr)snake_update, snake);
  if (snake->focused) {
    snake_tick(snake);
  }
  snake_paint_once(snake);
}

// Returns the direction the snake leaves cell. Out-of-bounds x,y return EMPTY.
Direction get_cell(Map* map, uint8_t x, uint8_t y) {
  if (!in_bounds(x, y)) {
    return EMPTY;
  }
  return map->cell[y][x];
}

void snake_paint_once(Snake *snake) {
  if (!snake->focused) {
    // don't paint on borrowed s4 surface
    return;
  }

  Screen4 *s4 = snake->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);
  Map* map = &snake->map;

  // paint horizontal segments
  for (uint8_t y=0; y<CANVAS_H; y++) {
    for (uint8_t x=0; x<CANVAS_W; x++) {
      uint8_t py = 2*y;
      uint8_t px = 2*x;
      raster_paint_pixel_v(rrect, px+1, py,
        get_cell(map, x, y)==RIGHT || get_cell(map, x+1, y)==LEFT);
      raster_paint_pixel_v(rrect, px, py+1,
        get_cell(map, x, y)==DOWN || get_cell(map, x, y+1)==UP);
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
}

UIEventDisposition snake_event_handler(UIEventHandler *raw_handler,
                                      UIEvent evt) {
  Snake *snake = ((SnakeHandler *)raw_handler)->snake;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case '1':
    case 'p':
      snake->direction = (snake->direction + 1) % 4;
      break;
    case '4':
    case 'q':
      snake->direction = (snake->direction - 1 + 4) % 4;
      break;
    case 'a':
      snake_advance_head(snake);
      break;
    case 'b':
      snake_advance_tail(snake);
      break;
    case uie_focus:
      if (!snake->focused) {
        s4_show(snake->s4);
        board_buffer_push(&snake->score_bbuf, snake->score_boardnum);
      }
      snake->focused = TRUE;
      break;
    case uie_escape:
      if (snake->focused) {
        s4_hide(snake->s4);
        board_buffer_pop(&snake->score_bbuf);
      }
      snake->focused = FALSE;
      result = uied_blur;
      break;
  }
  snake_paint_once(snake);
  return result;
}
