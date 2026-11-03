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
#include "weather_icon.h"

LV_IMAGE_DECLARE(icon_clear_day);
LV_IMAGE_DECLARE(icon_clear_night);
LV_IMAGE_DECLARE(icon_partly_cloudy_day);
LV_IMAGE_DECLARE(icon_partly_cloudy_night);
LV_IMAGE_DECLARE(icon_cloudy);
LV_IMAGE_DECLARE(icon_fog);
LV_IMAGE_DECLARE(icon_rain);
LV_IMAGE_DECLARE(icon_snow);
LV_IMAGE_DECLARE(icon_thunderstorms);

// by condition, then night (0) or day (1)
static const lv_image_dsc_t *const ICONS[WEATHER_CONDITION_COUNT][2] = {
    [WEATHER_CLEAR] = {&icon_clear_night, &icon_clear_day},
    [WEATHER_PARTLY_CLOUDY] = {&icon_partly_cloudy_night, &icon_partly_cloudy_day},
    [WEATHER_CLOUDY] = {&icon_cloudy, &icon_cloudy},
    [WEATHER_FOG] = {&icon_fog, &icon_fog},
    [WEATHER_RAIN] = {&icon_rain, &icon_rain},
    [WEATHER_SNOW] = {&icon_snow, &icon_snow},
    [WEATHER_THUNDERSTORM] = {&icon_thunderstorms, &icon_thunderstorms},
};

lv_obj_t *weather_icon_create(lv_obj_t *parent) {
  return lv_image_create(parent);
}

void weather_icon_set(lv_obj_t *icon, weather_condition_t condition, bool is_day) {
  lv_image_set_src(icon, ICONS[condition][is_day ? 1 : 0]);
}
