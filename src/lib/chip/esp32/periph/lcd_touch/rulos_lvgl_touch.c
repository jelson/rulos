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
#include "periph/lcd_touch/rulos_lvgl_touch.h"

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
  esp_lcd_touch_read_data(touch);
  esp_lcd_touch_point_data_t point;
  uint8_t count = 0;
  if (esp_lcd_touch_get_data(touch, &point, &count, 1) == ESP_OK && count > 0) {
    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

lv_indev_t *rulos_lvgl_add_touch(lv_display_t *display, esp_lcd_touch_handle_t touch) {
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, read_cb);
  lv_indev_set_user_data(indev, touch);
  lv_indev_set_display(indev, display);
  return indev;
}
