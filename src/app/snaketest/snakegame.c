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

#define PS(v) ((v) << PONG_SCALE2)
#define BALLDIA 3
#define PADDLEHEIGHT 8
#define SCORE_PAUSE_TIME (((Time)1) << 20)
#define BALLMINX PS(2)
#define BALLMAXX PS(29 - BALLDIA)
#define BALLMINY PS(0)
#define BALLMAXY PS(24 - BALLDIA)

void snake_serve(Snake *snake, uint8_t from_player);
void snake_update(Snake *snake);
void snake_paint_once(Snake *snake);
void snake_paint_paddle(Snake *snake, int x, int y);
void snake_score_one(Snake *snake, uint8_t player);
UIEventDisposition snake_event_handler(UIEventHandler *raw_handler, UIEvent evt);

void snake_init(Snake *snake, Screen4 *s4, AudioClient *audioClient) {
  snake->s4 = s4;
#if 0
	snake->board0 = b0;
	int i;
	for (i=0; i<PONG_HEIGHT; i++)
	{
		board_buffer_init(&snake->bbuf[i] DBG_BBUF_LABEL("snake"));
		board_buffer_push(&snake->bbuf[i], b0+i);
		snake->btable[i] = &snake->bbuf[i];
	}
	snake->rrect.bbuf = snake->btable;
	snake->rrect.ylen = PONG_HEIGHT;
	snake->rrect.x = 0;
	snake->rrect.xlen = 8;

#endif
  snake->handler.func = (UIEventHandlerFunc)snake_event_handler;
  snake->handler.snake = snake;

  snake_serve(snake, 0);

  snake->paddley[0] = 8;
  snake->paddley[1] = 10;

  snake->score[0] = 0;
  snake->score[1] = 0;
  snake->lastScore = clock_time_us();

  snake->audioClient = audioClient;

  snake->focused = FALSE;

  schedule_us(1, (ActivationFuncPtr)snake_update, snake);
}

void snake_serve(Snake *snake, uint8_t from_player) {
  snake->x = BALLMINX;
  snake->y = (deadbeef_rand() % (BALLMAXY - BALLMINY)) + BALLMINY;
  snake->dx = PS(((deadbeef_rand() & 7) + 7) << 1);
  snake->dy = PS(((deadbeef_rand() & 7) + 4) << 1);
  if (from_player) {
    snake->x = BALLMAXX;
    snake->dx = -snake->dx;
  }
}

uint8_t snake_is_paused(Snake *snake) {
  Time dl = clock_time_us() - snake->lastScore;
  return (0 < dl && dl < SCORE_PAUSE_TIME);
}

void snake_intersect_paddle(Snake *snake, uint8_t player, int by1) {
  int by0 = snake->y;
  if (by0 > by1) {
    int tmp = by1;
    by1 = by0;
    by0 = tmp;
  }
  int py0 = PS(snake->paddley[player]);
  int py1 = PS(snake->paddley[player] + PADDLEHEIGHT);
  if (by1 < py0 || by0 > py1) {
    ac_skip_to_clip(snake->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_score, sound_silence);
    snake_score_one(snake, 1 - player);
  } else {
    ac_skip_to_clip(snake->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_paddle_bounce, sound_silence);
    snake->dx += PS((deadbeef_rand() % 5) - 2);
    snake->dy += PS((deadbeef_rand() % 5) - 2);
  }
}

void snake_advance_ball(Snake *snake) {
  int newx = snake->x + (snake->dx >> PONG_FREQ2);
  int newy = snake->y + (snake->dy >> PONG_FREQ2);
  if (newx > BALLMAXX) {
    snake_intersect_paddle(snake, 1, newy);
    snake->dx = -snake->dx;
    newx = BALLMAXX - (newx - BALLMAXX);
  }
  if (newx < BALLMINX) {
    snake_intersect_paddle(snake, 0, newy);
    snake->dx = -snake->dx;
    newx = BALLMINX + (BALLMINX - newx);
  }
  if (newy > BALLMAXY) {
    snake->dy = -snake->dy;
    newy = BALLMAXY - (newy - BALLMAXY);
    ac_skip_to_clip(snake->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_wall_bounce, sound_silence);
  }
  if (newy < BALLMINY) {
    snake->dy = -snake->dy;
    newy = BALLMINY + (BALLMINY - newy);
    ac_skip_to_clip(snake->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_wall_bounce, sound_silence);
  }
  snake->x = newx;
  snake->y = newy;
}

void snake_score_one(Snake *snake, uint8_t player) {
  snake->lastScore = clock_time_us();
  snake->score[player] += 1;
  snake_serve(snake, player);
  snake_paint_once(snake);
}

void snake_update(Snake *snake) {
  schedule_us(1000000 / PONG_FREQ, (ActivationFuncPtr)snake_update, snake);
  if (!snake_is_paused(snake) && snake->focused) {
    snake_advance_ball(snake);
  }
  snake_paint_once(snake);
}

void snake_paint_once(Snake *snake) {
  if (!snake->focused) {
    // don't paint on borrowed s4 surface
    return;
  }

  Screen4 *s4 = snake->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);

  if (snake_is_paused(snake)) {
    ascii_to_bitmap_str(s4->bbuf[1].buffer + 1, 6, "P0  P1");

    char scoretext[3];
    int_to_string2(scoretext, 2, 0, snake->score[0] % 100);
    ascii_to_bitmap_str(s4->bbuf[2].buffer + 1, 2, scoretext);

    int_to_string2(scoretext, 2, 0, snake->score[1] % 100);
    ascii_to_bitmap_str(s4->bbuf[2].buffer + 5, 2, scoretext);
  } else {
    int xo, yo;
    for (xo = 0; xo < BALLDIA; xo++) {
      for (yo = 0; yo < BALLDIA; yo++) {
        raster_paint_pixel(rrect, (snake->x >> PONG_SCALE2) + xo,
                           (snake->y >> PONG_SCALE2) + yo);
      }
    }
  }

  snake_paint_paddle(snake, 0, snake->paddley[0]);
  snake_paint_paddle(snake, 28, snake->paddley[1]);

  raster_draw_buffers(rrect);
}

void snake_paint_paddle(Snake *snake, int x, int y) {
  RectRegion *rrect = &snake->s4->rrect;
  raster_paint_pixel(rrect, x + 1, y);
  raster_paint_pixel(rrect, x + 1, y + PADDLEHEIGHT);
  int yo;
  for (yo = 1; yo < PADDLEHEIGHT; yo++) {
    raster_paint_pixel(rrect, x + 0, y + yo);
    raster_paint_pixel(rrect, x + 2, y + yo);
  }
}

UIEventDisposition snake_event_handler(UIEventHandler *raw_handler,
                                      UIEvent evt) {
  Snake *snake = ((SnakeHandler *)raw_handler)->snake;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case '1':
    case 'p':
      snake->paddley[0] = max(snake->paddley[0] - 2, 0);
      break;
    case '4':
    case 'q':
      snake->paddley[0] = min(snake->paddley[0] + 2, 22 - PADDLEHEIGHT);
      break;
    case '3':
    case 'e':
      snake->paddley[1] = max(snake->paddley[1] - 2, 0);
      break;
    case '6':
    case 'f':
      snake->paddley[1] = min(snake->paddley[1] + 2, 22 - PADDLEHEIGHT);
      break;
    case uie_focus:
      if (!snake->focused) {
        s4_show(snake->s4);
      }
      snake->focused = TRUE;
      break;
    case uie_escape:
      if (snake->focused) {
        s4_hide(snake->s4);
      }
      snake->focused = FALSE;
      result = uied_blur;
      break;
  }
  snake_paint_once(snake);
  return result;
}
