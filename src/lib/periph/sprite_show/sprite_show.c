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

static void read_from_memory(void *ctx, uint32_t offset, uint32_t size, uint8_t *dst) {
  memcpy(dst, (const uint8_t *)ctx + offset, size);
}

sprite_blob_t sprite_blob_in_memory(const uint8_t *blob) {
  sprite_blob_t b = {read_from_memory, (void *)blob};
  return b;
}

// The current frame's image is read into the player's buffer, which every
// descriptor points at. A descriptor is only decoded while its image is
// the one in the buffer: LVGL decodes when it draws, and the player only
// changes the buffer when it changes the source.
static void show_frame(sprite_player_t *player) {
  const sprite_frame_t *frame = &player->show->frames[player->frame];
  const sprite_image_t *image = &player->show->images[frame->image];
  player->blob.read(player->blob.ctx, image->offset, image->size, player->buffer);
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
  uint32_t largest = 0;
  for (unsigned s = 0; s < num_shows; s++) {
    for (uint16_t i = 0; i < shows[s].num_images; i++) {
      if (shows[s].images[i].size > largest) {
        largest = shows[s].images[i].size;
      }
    }
  }
  player->buffer = (uint8_t *)lv_malloc(largest);
  LV_ASSERT_MALLOC(player->buffer);

  for (unsigned s = 0; s < num_shows; s++) {
    for (uint16_t i = 0; i < shows[s].num_images; i++) {
      const sprite_image_t *image = &shows[s].images[i];
      lv_image_dsc_t *dsc = &shows[s].dscs[i];
      dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
      dsc->header.cf = LV_COLOR_FORMAT_I8;
      dsc->header.flags = LV_IMAGE_FLAGS_COMPRESSED;
      dsc->header.w = image->width;
      dsc->header.h = image->height;
      dsc->header.stride = image->width;
      dsc->data_size = image->size;
      dsc->data = player->buffer;
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
