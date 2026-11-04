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

void sprite_show_bind(const sprite_show_t *show, const uint8_t *blob) {
  for (uint16_t i = 0; i < show->num_images; i++) {
    const sprite_image_t *image = &show->images[i];
    lv_image_dsc_t *dsc = &show->dscs[i];
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_I8;
    dsc->header.flags = LV_IMAGE_FLAGS_COMPRESSED;
    dsc->header.w = image->width;
    dsc->header.h = image->height;
    dsc->header.stride = image->width;
    dsc->data_size = image->size;
    dsc->data = blob + image->offset;
  }
}

static void show_frame(sprite_player_t *player) {
  const sprite_frame_t *frame = &player->show->frames[player->frame];
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

void sprite_player_init(sprite_player_t *player, lv_obj_t *parent) {
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
