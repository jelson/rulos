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

#include "periph/rocket/pong.h"

#include "core/rulos.h"
#include "periph/audio/sound.h"
#include "periph/rasters/rasters.h"
#include "periph/rocket/screen4.h"

#define PS(v)            ((v) << PONG_SCALE2)
#define BALLDIA          3
#define PADDLEHEIGHT     8
#define SCORE_PAUSE_TIME (((Time)1) << 20)
#define BALLMINX         PS(2)
#define BALLMAXX         PS(29 - BALLDIA)
#define BALLMINY         PS(0)
#define BALLMAXY         PS((SCREEN4SIZE * 6) - BALLDIA)
#define PADDLEMAXY       ((SCREEN4SIZE * 6) - PADDLEHEIGHT - 2)

void pong_serve(Pong *pong, uint8_t from_player);
void pong_update(Pong *pong);
void pong_paint_once(Pong *pong);
void pong_paint_paddle(Pong *pong, int x, int y);
void pong_score_one(Pong *pong, uint8_t player);
UIEventDisposition pong_event_handler(UIEventHandler *raw_handler, UIEvent evt);

void pong_init(Pong *pong, Screen4 *s4, AudioClient *audioClient) {
  pong->s4 = s4;
  pong->handler.func = (UIEventHandlerFunc)pong_event_handler;
  pong->handler.pong = pong;

  pong_serve(pong, 0);

  pong->paddley[0] = 8;
  pong->paddley[1] = 10;

  pong->score[0] = 0;
  pong->score[1] = 0;
  pong->lastScore = clock_time_us();

  pong->audioClient = audioClient;

  pong->focused = FALSE;

  schedule_us(1, (ActivationFuncPtr)pong_update, pong);
}

void pong_serve(Pong *pong, uint8_t from_player) {
  pong->x = BALLMINX;
  pong->y = (deadbeef_rand() % (BALLMAXY - BALLMINY)) + BALLMINY;
  pong->dx = PS(((deadbeef_rand() & 7) + 7) << 1);
  pong->dy = PS(((deadbeef_rand() & 7) + 4) << 1);
  if (from_player) {
    pong->x = BALLMAXX;
    pong->dx = -pong->dx;
  }
}

uint8_t pong_is_paused(Pong *pong) {
  Time dl = clock_time_us() - pong->lastScore;
  return (0 < dl && dl < SCORE_PAUSE_TIME);
}

void pong_intersect_paddle(Pong *pong, uint8_t player, int by1) {
  int by0 = pong->y;
  if (by0 > by1) {
    int tmp = by1;
    by1 = by0;
    by0 = tmp;
  }
  int py0 = PS(pong->paddley[player]);
  int py1 = PS(pong->paddley[player] + PADDLEHEIGHT);
  if (by1 < py0 || by0 > py1) {
    ac_skip_to_clip(pong->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_score, sound_silence);
    pong_score_one(pong, 1 - player);
  } else {
    ac_skip_to_clip(pong->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_paddle_bounce, sound_silence);
    pong->dx += PS((deadbeef_rand() % 5) - 2);
    pong->dy += PS((deadbeef_rand() % 5) - 2);
  }
}

void pong_advance_ball(Pong *pong) {
  int newx = pong->x + (pong->dx >> PONG_FREQ2);
  int newy = pong->y + (pong->dy >> PONG_FREQ2);
  if (newx > BALLMAXX) {
    pong_intersect_paddle(pong, 1, newy);
    pong->dx = -pong->dx;
    newx = BALLMAXX - (newx - BALLMAXX);
  }
  if (newx < BALLMINX) {
    pong_intersect_paddle(pong, 0, newy);
    pong->dx = -pong->dx;
    newx = BALLMINX + (BALLMINX - newx);
  }
  if (newy > BALLMAXY) {
    pong->dy = -pong->dy;
    newy = BALLMAXY - (newy - BALLMAXY);
    ac_skip_to_clip(pong->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_wall_bounce, sound_silence);
  }
  if (newy < BALLMINY) {
    pong->dy = -pong->dy;
    newy = BALLMINY + (BALLMINY - newy);
    ac_skip_to_clip(pong->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_pong_wall_bounce, sound_silence);
  }
  pong->x = newx;
  pong->y = newy;
}

void pong_score_one(Pong *pong, uint8_t player) {
  pong->lastScore = clock_time_us();
  pong->score[player] += 1;
  pong_serve(pong, player);
  pong_paint_once(pong);
}

void pong_update(Pong *pong) {
  schedule_us(1000000 / PONG_FREQ, (ActivationFuncPtr)pong_update, pong);
  if (!pong_is_paused(pong) && pong->focused) {
    pong_advance_ball(pong);
  }
  pong_paint_once(pong);
}

void pong_paint_once(Pong *pong) {
  if (!pong->focused) {
    // don't paint on borrowed s4 surface
    return;
  }

  Screen4 *s4 = pong->s4;
  RectRegion *rrect = &s4->rrect;
  raster_clear_buffers(rrect);

  if (pong_is_paused(pong)) {
    ascii_to_bitmap_str(s4->bbuf[1].buffer + 1, 6, "P0  P1");

    char scoretext[3];
    int_to_string2(scoretext, 2, 0, pong->score[0] % 100);
    ascii_to_bitmap_str(s4->bbuf[2].buffer + 1, 2, scoretext);

    int_to_string2(scoretext, 2, 0, pong->score[1] % 100);
    ascii_to_bitmap_str(s4->bbuf[2].buffer + 5, 2, scoretext);
  } else {
    int xo, yo;
    for (xo = 0; xo < BALLDIA; xo++) {
      for (yo = 0; yo < BALLDIA; yo++) {
        raster_paint_pixel(rrect, (pong->x >> PONG_SCALE2) + xo,
                           (pong->y >> PONG_SCALE2) + yo);
      }
    }
  }

  pong_paint_paddle(pong, 0, pong->paddley[0]);
  pong_paint_paddle(pong, 28, pong->paddley[1]);

  raster_draw_buffers(rrect);
}

void pong_paint_paddle(Pong *pong, int x, int y) {
  RectRegion *rrect = &pong->s4->rrect;
  raster_paint_pixel(rrect, x + 1, y);
  raster_paint_pixel(rrect, x + 1, y + PADDLEHEIGHT);
  for (int yo = 1; yo < PADDLEHEIGHT; yo++) {
    raster_paint_pixel(rrect, x + 0, y + yo);
    raster_paint_pixel(rrect, x + 2, y + yo);
  }
}

UIEventDisposition pong_event_handler(UIEventHandler *raw_handler,
                                      UIEvent evt) {
  Pong *pong = ((PongHandler *)raw_handler)->pong;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case '1':
    case 'm':
      pong->paddley[0] = r_max(pong->paddley[0] - 2, 0);
      break;
    case '4':
    case 'n':
      pong->paddley[0] = r_min(pong->paddley[0] + 2, PADDLEMAXY);
      break;
    case '3':
    case 'e':
      pong->paddley[1] = r_max(pong->paddley[1] - 2, 0);
      break;
    case '6':
    case 'f':
      pong->paddley[1] = r_min(pong->paddley[1] + 2, PADDLEMAXY);
      break;
    case uie_focus:
      if (!pong->focused) {
        s4_show(pong->s4);
      }
      pong->focused = TRUE;
      break;
    case uie_escape:
      if (pong->focused) {
        s4_hide(pong->s4);
      }
      pong->focused = FALSE;
      result = uied_blur;
      break;
  }
  pong_paint_once(pong);
  return result;
}
