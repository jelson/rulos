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
#include "periph/lvgl/rulos_lvgl.h"

#include "core/rulos.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"

// how often LVGL gets to run its timers (animations, refresh)
#define LVGL_HANDLER_PERIOD_US 5000

static void run_lvgl_timers(void *data) {
  schedule_us(LVGL_HANDLER_PERIOD_US, run_lvgl_timers, NULL);
  lv_timer_handler();
}

static uint32_t tick_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

void rulos_lvgl_init(void) {
  lv_init();
  lv_tick_set_cb(tick_ms);
  schedule_now(run_lvgl_timers, NULL);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  // In direct mode px_map is the frame buffer LVGL just finished drawing,
  // so for the panel this is a buffer switch rather than a copy.
  esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, lv_display_get_horizontal_resolution(disp),
                                            lv_display_get_vertical_resolution(disp), px_map));
  lv_display_flush_ready(disp);
}

lv_display_t *rulos_lvgl_add_rgb_panel(const esp_lcd_rgb_panel_config_t *config) {
  assert(config->num_fbs == 2);
  assert(config->bits_per_pixel == 16);

  esp_lcd_panel_handle_t panel;
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(config, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

  void *fb0;
  void *fb1;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1));

  const uint32_t h_res = config->timings.h_res;
  const uint32_t v_res = config->timings.v_res;
  lv_display_t *disp = lv_display_create(h_res, v_res);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, fb0, fb1, h_res * v_res * sizeof(uint16_t),
                         LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_user_data(disp, panel);
  return disp;
}
