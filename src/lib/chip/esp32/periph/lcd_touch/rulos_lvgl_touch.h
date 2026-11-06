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

#include "esp_lcd_touch.h"
#include "lvgl.h"

// Registers a touch controller driven by Espressif's esp_lcd_touch (GT911,
// FT5x06, and so on) as an LVGL pointer on the display. LVGL polls it from
// its own timers, on the thread rulos_lvgl runs them.
lv_indev_t *rulos_lvgl_add_touch(lv_display_t *display, esp_lcd_touch_handle_t touch);
