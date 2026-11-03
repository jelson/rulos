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

#include "lvgl.h"
#include "periph/weather/weather.h"

// Weather icons: Meteocons by Bas Milius (MIT), embedded as PNGs and
// decoded by LVGL on first use. See icons/.

// Creates an icon, showing nothing yet, on parent.
lv_obj_t *weather_icon_create(lv_obj_t *parent);

// Shows a condition in the icon.
void weather_icon_set(lv_obj_t *icon, weather_condition_t condition, bool is_day);
