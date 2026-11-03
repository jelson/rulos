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
// The clock on a PC: the same screen as the device, fed from the system
// clock, with the weather fetched through curl. With SDL2 it opens a
// window; with --screenshot it renders one frame to a BMP file and exits,
// which needs no display at all.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "clock_config.h"
#include "clock_ui.h"
#include "lvgl.h"
#include "periph/openmeteo/openmeteo.h"
#include "weather_icon.h"

#define HOR_RES           800
#define VER_RES           480
#define FRAME_PERIOD_US   5000
#define WEATHER_REPLY_LEN 2048

static uint32_t tick_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void show_local_time(void) {
  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  clock_ui_set_time(&local);
}

// Fetches the weather with curl and shows it. Returns whether it worked.
static bool show_weather(void) {
  char url[OPENMETEO_URL_LEN];
  openmeteo_url(url, sizeof(url), CLOCK_LATITUDE, CLOCK_LONGITUDE, CLOCK_FAHRENHEIT);
  char command[OPENMETEO_URL_LEN + 64];
  snprintf(command, sizeof(command), "curl --silent --max-time 10 '%s'", url);

  char reply[WEATHER_REPLY_LEN];
  FILE *curl = popen(command, "r");
  if (curl == NULL) {
    perror("curl");
    return false;
  }
  const size_t len = fread(reply, 1, sizeof(reply) - 1, curl);
  reply[len] = '\0';
  pclose(curl);

  weather_t weather;
  const bool ok = openmeteo_parse(reply, &weather);
  clock_ui_set_weather(ok ? &weather : NULL);
  if (!ok) {
    fprintf(stderr, "weather: could not parse reply: %s\n", reply);
  }
  return ok;
}

//////////////// screenshot /////////////////////

static void put_le32(uint8_t *p, uint32_t v) {
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

// Writes an RGB565 frame as a 24-bit BMP.
static int write_bmp(const char *path, const lv_color16_t *frame, int width, int height) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    perror(path);
    return 1;
  }
  const uint32_t row_bytes = (width * 3 + 3) & ~3u;
  const uint32_t header_bytes = 54;
  uint8_t header[54] = {'B', 'M'};
  put_le32(header + 2, header_bytes + row_bytes * height);
  put_le32(header + 10, header_bytes);
  put_le32(header + 14, 40);  // BITMAPINFOHEADER
  put_le32(header + 18, width);
  put_le32(header + 22, height);
  header[26] = 1;   // planes
  header[28] = 24;  // bits per pixel
  put_le32(header + 34, row_bytes * height);
  fwrite(header, 1, sizeof(header), f);

  uint8_t row[row_bytes];
  memset(row, 0, sizeof(row));
  for (int y = height - 1; y >= 0; y--) {
    for (int x = 0; x < width; x++) {
      const lv_color16_t c = frame[y * width + x];
      row[x * 3 + 0] = (c.blue << 3) | (c.blue >> 2);
      row[x * 3 + 1] = (c.green << 2) | (c.green >> 4);
      row[x * 3 + 2] = (c.red << 3) | (c.red >> 2);
    }
    fwrite(row, 1, row_bytes, f);
  }
  return fclose(f) == 0 ? 0 : 1;
}

static void discard_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  lv_display_flush_ready(disp);
}

static lv_color16_t frame[HOR_RES * VER_RES];

// A display that renders into frame and goes nowhere.
static lv_display_t *memory_display(void) {
  lv_display_t *disp = lv_display_create(HOR_RES, VER_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, frame, NULL, sizeof(frame), LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, discard_flush);
  return disp;
}

static int screenshot(const char *path) {
  lv_display_t *disp = memory_display();
  clock_ui_create();
  show_local_time();
  show_weather();
  clock_ui_set_status("Rendered on the PC");
  lv_refr_now(disp);
  return write_bmp(path, frame, HOR_RES, VER_RES);
}

// Every weather icon, by day and by night, for checking the artwork.
static int icon_gallery(const char *path) {
  lv_display_t *disp = memory_display();
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  const int32_t cell = HOR_RES / WEATHER_CONDITION_COUNT;
  const int32_t row_height = 160;
  for (int condition = 0; condition < WEATHER_CONDITION_COUNT; condition++) {
    for (int is_day = 0; is_day < 2; is_day++) {
      lv_obj_t *icon = weather_icon_create(screen);
      weather_icon_set(icon, (weather_condition_t)condition, is_day);
      lv_obj_set_pos(icon, condition * cell + (cell - 128) / 2, 20 + (1 - is_day) * row_height);
    }
    lv_obj_t *name = lv_label_create(screen);
    lv_label_set_text(name, weather_condition_name((weather_condition_t)condition));
    lv_obj_set_style_text_color(name, lv_color_white(), 0);
    lv_obj_set_width(name, cell);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(name, condition * cell, 20 + 2 * row_height);
  }
  lv_refr_now(disp);
  return write_bmp(path, frame, HOR_RES, VER_RES);
}

//////////////// main /////////////////////

static int usage(const char *argv0) {
  fprintf(stderr, "usage: %s [--screenshot FILE.bmp | --icons FILE.bmp]\n", argv0);
  return 1;
}

int main(int argc, char **argv) {
  lv_init();
  lv_tick_set_cb(tick_ms);

  if (argc == 3 && strcmp(argv[1], "--screenshot") == 0) {
    return screenshot(argv[2]);
  }
  if (argc == 3 && strcmp(argv[1], "--icons") == 0) {
    return icon_gallery(argv[2]);
  }
  if (argc != 1) {
    return usage(argv[0]);
  }

#if LV_USE_SDL
  lv_display_t *disp = lv_sdl_window_create(HOR_RES, VER_RES);
  lv_sdl_window_set_title(disp, "ntpclock");
  clock_ui_create();
  clock_ui_set_status("Running on the PC");
  time_t next_weather_fetch = 0;
  while (1) {
    show_local_time();
    if (time(NULL) >= next_weather_fetch) {
      const bool ok = show_weather();
      next_weather_fetch = time(NULL) + (ok ? CLOCK_WEATHER_PERIOD_SEC : CLOCK_WEATHER_RETRY_SEC);
    }
    lv_timer_handler();
    usleep(FRAME_PERIOD_US);
  }
#else
  fprintf(stderr, "built without SDL2 (install libsdl2-dev); only --screenshot works\n");
  return 1;
#endif
}
