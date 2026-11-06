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

#include "core/hardware.h"
#include "core/rulos.h"
#include "esp_attr.h"
#include "esp_ipc.h"
#include "esp_lcd_panel_ops.h"
#include "esp_random.h"
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
  // lv_rand() otherwise repeats the same sequence from every boot
  lv_rand_set_seed(esp_random());
  schedule_now(run_lvgl_timers, NULL);
}

// The panel takes a buffer switch at the start of its next frame, and
// reports it here, from its interrupt. Until then the buffer LVGL wants to
// draw into next is still the one being scanned out.
static volatile bool frame_buffer_switched = true;

static bool IRAM_ATTR on_frame_buf_complete(esp_lcd_panel_handle_t panel,
                                            const esp_lcd_rgb_panel_event_data_t *event,
                                            void *user_ctx) {
  frame_buffer_switched = true;
  return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  // In direct mode px_map is the frame buffer LVGL just finished drawing,
  // so for the panel this is a buffer switch rather than a copy. LVGL
  // draws its next frame into the other buffer, so the previous switch
  // must have taken effect first; by the time LVGL refreshes again it
  // almost always has.
  esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
  while (!frame_buffer_switched) {
  }
  frame_buffer_switched = false;
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, lv_display_get_horizontal_resolution(disp),
                                            lv_display_get_vertical_resolution(disp), px_map));
  lv_display_flush_ready(disp);
}

typedef struct {
  const esp_lcd_rgb_panel_config_t *config;
  esp_lcd_panel_handle_t panel;
} new_panel_args_t;

static void new_panel(void *arg) {
  new_panel_args_t *args = (new_panel_args_t *)arg;
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(args->config, &args->panel));
}

lv_display_t *rulos_lvgl_add_rgb_panel(const esp_lcd_rgb_panel_config_t *config) {
  assert(config->num_fbs == 2);
  assert(config->bits_per_pixel == 16);

  // The panel's interrupt is allocated on the core that creates it. That
  // interrupt feeds the panel from the PSRAM frame buffer every few
  // scanlines, so it goes on the other core from RULOS and LVGL, which
  // would otherwise hold it up while they draw.
  new_panel_args_t args = {config, NULL};
  ESP_ERROR_CHECK(esp_ipc_call_blocking(RULOS_ESP32_CORE_ID == 0 ? 1 : 0, new_panel, &args));
  esp_lcd_panel_handle_t panel = args.panel;
  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_frame_buf_complete = on_frame_buf_complete;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL));
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
