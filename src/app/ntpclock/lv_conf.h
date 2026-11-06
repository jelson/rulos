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
// LVGL configuration for the clock, shared by the device and host builds.
// Anything not set here takes LVGL's default.

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// The system allocator rather than LVGL's fixed pool: on the ESP32-S3
// large allocations then come from PSRAM.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 33
#define LV_USE_OS          LV_OS_NONE

#define LV_USE_LOG    1
#define LV_LOG_LEVEL  LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

// Weather icons are PNGs decoded on first use and kept decoded. (The cat's
// frames are decoded by the sprite player itself and are never cached.)
#define LV_USE_LODEPNG    1
#define LV_CACHE_DEF_SIZE (512 * 1024)

#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT       &lv_font_montserrat_20

// The host build defines LV_USE_SDL when SDL2 is installed
#ifndef LV_USE_SDL
#define LV_USE_SDL 0
#endif
#if LV_USE_SDL
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#endif

#endif  // LV_CONF_H
