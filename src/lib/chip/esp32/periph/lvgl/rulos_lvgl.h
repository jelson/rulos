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

#include "esp_lcd_panel_rgb.h"
#include "lvgl.h"

// Starts LVGL and drives it from the RULOS scheduler: LVGL's tick is the
// IDF high-resolution timer and lv_timer_handler runs as a periodic RULOS
// task, so the UI code shares the scheduler's thread and needs no locking.
void rulos_lvgl_init(void);

// Brings up an RGB panel and registers it as an LVGL display. The panel
// must be configured with two frame buffers (num_fbs = 2): LVGL renders
// straight into them, and each flush makes the newly drawn one the one
// being scanned out.
lv_display_t *rulos_lvgl_add_rgb_panel(const esp_lcd_rgb_panel_config_t *config);
