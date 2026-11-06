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
#include "periph/sprite_show/sprite_show.h"

#include <string.h>

// An image in the blob: a 12-byte header (compression method, compressed
// size, decompressed size) and then an RLE stream of a 256-entry BGRA
// palette followed by one byte per pixel.
#define IMAGE_HEADER_BYTES 12
#define PALETTE_ENTRIES    256
#define PALETTE_BYTES      (PALETTE_ENTRIES * 4)

static void read_from_memory(void *ctx, uint32_t offset, uint32_t size, uint8_t *dst) {
  memcpy(dst, (const uint8_t *)ctx + offset, size);
}

sprite_blob_t sprite_blob_in_memory(const uint8_t *blob) {
  sprite_blob_t b = {read_from_memory, (void *)blob};
  return b;
}

// Decodes the compressed image in the player's input buffer into its image
// buffer as RGB565 followed by an alpha plane, which LVGL blends as it is.
// The stream is LVGL's RLE: a control byte under 0x80 repeats the byte
// after it that many times; one with the high bit set is followed by that
// many literal bytes. The palette comes first and is turned into lookup
// tables as it goes by.
static void decode(sprite_player_t *player, const sprite_image_t *image) {
  const uint8_t *in = player->input + IMAGE_HEADER_BYTES;
  const uint8_t *const end = player->input + image->size;
  const uint32_t pixels = (uint32_t)image->width * image->height;
  uint16_t *color = (uint16_t *)player->image_buffer;
  uint8_t *alpha = player->image_buffer + pixels * 2;

  uint8_t palette[PALETTE_BYTES];
  uint16_t rgb565[PALETTE_ENTRIES];
  uint8_t opacity[PALETTE_ENTRIES];
  uint32_t pos = 0;  // bytes of the decompressed stream produced so far

  while (in < end) {
    const uint8_t control = *in++;
    const bool literal = control & 0x80;
    uint32_t count = literal ? control & 0x7f : control;
    const uint8_t repeated = literal ? 0 : *in++;

    // the palette, and the moment it is complete
    while (count > 0 && pos < PALETTE_BYTES) {
      palette[pos++] = literal ? *in++ : repeated;
      count--;
      if (pos == PALETTE_BYTES) {
        for (int i = 0; i < PALETTE_ENTRIES; i++) {
          const uint8_t b = palette[i * 4], g = palette[i * 4 + 1], r = palette[i * 4 + 2];
          rgb565[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
          opacity[i] = palette[i * 4 + 3];
        }
      }
    }

    if (count == 0) {
      continue;  // this run ended inside the palette
    }

    // pixels
    uint32_t p = pos - PALETTE_BYTES;
    if (p + count > pixels) {
      count = pixels - p;
    }
    if (literal) {
      for (uint32_t i = 0; i < count; i++, p++) {
        const uint8_t index = *in++;
        color[p] = rgb565[index];
        alpha[p] = opacity[index];
      }
    } else {
      const uint16_t c = rgb565[repeated];
      memset(alpha + p, opacity[repeated], count);
      for (uint32_t i = 0; i < count; i++, p++) {
        color[p] = c;
      }
    }
    pos = PALETTE_BYTES + p;
  }
}

static void show_frame(sprite_player_t *player) {
  const sprite_frame_t *frame = &player->show->frames[player->frame];
  const sprite_image_t *image = &player->show->images[frame->image];
  player->blob.read(player->blob.ctx, image->offset, image->size, player->input);
  decode(player, image);
  lv_image_set_src(player->image, &player->show->dscs[frame->image]);
  lv_obj_set_pos(player->image, frame->x, frame->y);
}

static void step(lv_timer_t *timer) {
  sprite_player_t *player = (sprite_player_t *)lv_timer_get_user_data(timer);
  player->frame++;
  if (player->frame >= player->show->num_frames) {
    player->show = NULL;
    lv_obj_add_flag(player->image, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
    return;
  }
  show_frame(player);
}

void sprite_player_init(sprite_player_t *player, lv_obj_t *parent, const sprite_blob_t *blob,
                        const sprite_show_t *shows, unsigned num_shows) {
  player->blob = *blob;
  uint32_t largest_input = 0;
  uint32_t largest_pixels = 0;
  for (unsigned s = 0; s < num_shows; s++) {
    for (uint16_t i = 0; i < shows[s].num_images; i++) {
      const sprite_image_t *image = &shows[s].images[i];
      if (image->size > largest_input) {
        largest_input = image->size;
      }
      if ((uint32_t)image->width * image->height > largest_pixels) {
        largest_pixels = (uint32_t)image->width * image->height;
      }
    }
  }
  player->input = (uint8_t *)lv_malloc(largest_input);
  LV_ASSERT_MALLOC(player->input);
  player->image_buffer = (uint8_t *)lv_malloc(largest_pixels * 3);
  LV_ASSERT_MALLOC(player->image_buffer);

  // The descriptors all name the one image buffer, which holds the image
  // of whichever frame is showing. LVGL draws an uncompressed image in
  // memory straight from its buffer, so nothing is cached or copied.
  for (unsigned s = 0; s < num_shows; s++) {
    for (uint16_t i = 0; i < shows[s].num_images; i++) {
      const sprite_image_t *image = &shows[s].images[i];
      lv_image_dsc_t *dsc = &shows[s].dscs[i];
      memset(dsc, 0, sizeof(*dsc));
      dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
      dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
      dsc->header.w = image->width;
      dsc->header.h = image->height;
      dsc->header.stride = image->width * 2;
      dsc->data_size = (uint32_t)image->width * image->height * 3;
      dsc->data = player->image_buffer;
    }
  }

  player->image = lv_image_create(parent);
  lv_obj_add_flag(player->image, LV_OBJ_FLAG_HIDDEN);
  player->timer = lv_timer_create(step, 1000, player);
  lv_timer_pause(player->timer);
  player->show = NULL;
}

void sprite_player_play(sprite_player_t *player, const sprite_show_t *show) {
  player->show = show;
  player->frame = 0;
  show_frame(player);
  lv_obj_clear_flag(player->image, LV_OBJ_FLAG_HIDDEN);
  lv_timer_set_period(player->timer, 1000 / show->fps);
  lv_timer_reset(player->timer);
  lv_timer_resume(player->timer);
}

bool sprite_player_is_playing(const sprite_player_t *player) {
  return player->show != NULL;
}
