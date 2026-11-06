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
#pragma once

// Pre-rendered sprite animations ("shows") for LVGL. A show is a sequence
// of frames, each naming an image and where its top-left corner goes on
// the screen. The images are LVGL binary images packed into one blob,
// which the build tool LvglSpriteShows() writes along with the tables
// below. The blob may live anywhere (a flash partition, a file, the
// program itself): the player reads each frame's image out of it through
// a sprite_blob_t into a buffer as the frame comes up.

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

// An image in the blob.
typedef struct {
  uint32_t offset;
  uint32_t size;
  uint16_t width;
  uint16_t height;
} sprite_image_t;

typedef struct {
  uint16_t image;
  int16_t x;
  int16_t y;
} sprite_frame_t;

typedef struct {
  const char *name;
  const sprite_image_t *images;
  uint16_t num_images;
  const sprite_frame_t *frames;
  uint16_t num_frames;
  uint16_t fps;
  // One descriptor per image, filled in by the player. Each image needs
  // its own because LVGL caches decoded images by descriptor address.
  lv_image_dsc_t *dscs;
} sprite_show_t;

// Access to the blob: copies size bytes starting at offset into dst.
typedef struct {
  void (*read)(void *ctx, uint32_t offset, uint32_t size, uint8_t *dst);
  void *ctx;
} sprite_blob_t;

// A blob that is already in memory.
sprite_blob_t sprite_blob_in_memory(const uint8_t *blob);

// Plays shows on one image object. The parent must not scroll, since a
// frame may lie partly outside it.
typedef struct {
  lv_obj_t *image;
  lv_timer_t *timer;
  sprite_blob_t blob;
  uint8_t *buffer;  // holds the current frame's image
  const sprite_show_t *show;
  uint16_t frame;
} sprite_player_t;

// Prepares the shows to play from the blob; the buffer is sized for the
// largest of their images.
void sprite_player_init(sprite_player_t *player, lv_obj_t *parent, const sprite_blob_t *blob,
                        const sprite_show_t *shows, unsigned num_shows);

// Starts a show from its first frame, replacing any show in progress.
void sprite_player_play(sprite_player_t *player, const sprite_show_t *show);

bool sprite_player_is_playing(const sprite_player_t *player);
