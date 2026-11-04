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
#include "clock_ui.h"

#include <stdio.h>
#include <string.h>

#include "cat_shows.h"
#include "lvgl.h"
#include "periph/sprite_show/sprite_show.h"
#include "weather_icon.h"

// montserrat_48 is the largest font LVGL ships, so the time is drawn
// scaled up (LV_SCALE_NONE is 1:1)
#define TIME_SCALE           (LV_SCALE_NONE * 3)
#define TIME_Y               (-95)
#define DATE_Y               25
#define WEATHER_Y            135
#define STATUS_BOTTOM_MARGIN 12

// Each character of the time gets a cell of fixed width, so the display
// doesn't shift as digits of different widths come and go.
#define TIME_PLACEHOLDER "--:--:--"
#define TIME_CELLS       (sizeof(TIME_PLACEHOLDER) - 1)
#define DIGIT_CELL_WIDTH 32
#define COLON_CELL_WIDTH 14

#define COLOR_TIME        lv_color_white()
#define COLOR_DATE        lv_color_hex(0x89B4FA)
#define COLOR_CONDITION   lv_color_white()
#define COLOR_TEMPERATURE lv_color_white()
#define COLOR_HIGH        lv_color_hex(0xF9A66C)
#define COLOR_LOW         lv_color_hex(0x7DCFFF)
#define COLOR_STATUS      lv_color_hex(0x6C7086)

static lv_obj_t *time_cells[TIME_CELLS];
static lv_obj_t *date_label;
static lv_obj_t *weather_row;
static lv_obj_t *weather_icon;
static lv_obj_t *condition_label;
static lv_obj_t *temperature_label;
static lv_obj_t *high_label;
static lv_obj_t *low_label;
static lv_obj_t *status_label;

// what the icon currently shows, so it is only redrawn on a change
static bool icon_valid;
static weather_condition_t icon_condition;
static bool icon_is_day;

// the cat, and which quarter hour she last performed in
static sprite_player_t cat;
static int last_quarter = -1;
static unsigned last_show;

static const char *const WEEKDAYS[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
static const char *const MONTHS[] = {"January",   "February", "March",    "April",
                                     "May",       "June",     "July",     "August",
                                     "September", "October",  "November", "December"};

// Style-based alignment, so an object stays put as its content changes
// size.
static void place(lv_obj_t *obj, lv_align_t align, int32_t y) {
  lv_obj_set_align(obj, align);
  lv_obj_set_y(obj, y);
}

// A bare container laying its children out in a row or column.
static lv_obj_t *flex_box(lv_obj_t *parent, lv_flex_flow_t flow, int32_t gap) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(box, flow);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(box, gap, 0);
  lv_obj_set_style_pad_row(box, gap, 0);
  return box;
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_color(obj, color, 0);
  return obj;
}

// Setting a label's text invalidates it even when nothing changed, which
// would redraw the whole screen on every call.
static void set_label(lv_obj_t *obj, const char *text) {
  if (strcmp(lv_label_get_text(obj), text) != 0) {
    lv_label_set_text(obj, text);
  }
}

static void create_time(lv_obj_t *screen) {
  lv_obj_t *row = flex_box(screen, LV_FLEX_FLOW_ROW, 0);
  place(row, LV_ALIGN_CENTER, TIME_Y);
  lv_obj_set_style_transform_scale_x(row, TIME_SCALE, 0);
  lv_obj_set_style_transform_scale_y(row, TIME_SCALE, 0);
  lv_obj_set_style_transform_pivot_x(row, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(row, lv_pct(50), 0);

  for (size_t i = 0; i < TIME_CELLS; i++) {
    lv_obj_t *cell = label(row, &lv_font_montserrat_48, COLOR_TIME);
    const bool colon = TIME_PLACEHOLDER[i] == ':';
    lv_obj_set_width(cell, colon ? COLON_CELL_WIDTH : DIGIT_CELL_WIDTH);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    time_cells[i] = cell;
  }
}

static void create_weather(lv_obj_t *screen) {
  weather_row = flex_box(screen, LV_FLEX_FLOW_ROW, 24);
  place(weather_row, LV_ALIGN_CENTER, WEATHER_Y);
  weather_icon = weather_icon_create(weather_row);

  lv_obj_t *text = flex_box(weather_row, LV_FLEX_FLOW_COLUMN, 6);
  condition_label = label(text, &lv_font_montserrat_28, COLOR_CONDITION);
  lv_obj_t *temperatures = flex_box(text, LV_FLEX_FLOW_ROW, 18);
  temperature_label = label(temperatures, &lv_font_montserrat_28, COLOR_TEMPERATURE);
  high_label = label(temperatures, &lv_font_montserrat_28, COLOR_HIGH);
  low_label = label(temperatures, &lv_font_montserrat_28, COLOR_LOW);
}

void clock_ui_create(const uint8_t *cat_frames) {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  // the cat walks in from beyond the edges; the screen must not scroll
  // after her
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  create_time(screen);
  date_label = label(screen, &lv_font_montserrat_28, COLOR_DATE);
  place(date_label, LV_ALIGN_CENTER, DATE_Y);
  create_weather(screen);
  status_label = label(screen, &lv_font_montserrat_20, COLOR_STATUS);
  place(status_label, LV_ALIGN_BOTTOM_MID, -STATUS_BOTTOM_MARGIN);

  // the cat is created last so she walks in front of everything
  for (unsigned i = 0; i < cat_show_count; i++) {
    sprite_show_bind(&cat_shows[i], cat_frames);
  }
  sprite_player_init(&cat, screen);

  clock_ui_set_time(NULL);
  clock_ui_set_weather(NULL);
  clock_ui_set_status("");
}

static void play_show(unsigned index) {
  last_show = index;
  sprite_player_play(&cat, &cat_shows[index]);
}

// A show at the top of each quarter hour, never the same one twice running.
static void quarter_hour_show(const struct tm *t) {
  const int quarter = (t->tm_yday * 24 + t->tm_hour) * 4 + t->tm_min / 15;
  if (t->tm_min % 15 != 0 || quarter == last_quarter) {
    return;
  }
  last_quarter = quarter;
  unsigned index = lv_rand(0, cat_show_count - 1);
  if (index == last_show) {
    index = (index + 1) % cat_show_count;
  }
  play_show(index);
}

bool clock_ui_play_show(const char *name) {
  for (unsigned i = 0; i < cat_show_count; i++) {
    if (strcmp(cat_shows[i].name, name) == 0) {
      play_show(i);
      return true;
    }
  }
  return false;
}

static void set_time_text(const char *text) {
  for (size_t i = 0; i < TIME_CELLS; i++) {
    const char cell_text[2] = {text[i], '\0'};
    set_label(time_cells[i], cell_text);
  }
}

void clock_ui_set_time(const struct tm *t) {
  if (t == NULL) {
    set_time_text(TIME_PLACEHOLDER);
    set_label(date_label, "");
    return;
  }
  char text[64];
  snprintf(text, sizeof(text), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  set_time_text(text);
  snprintf(text, sizeof(text), "%s, %s %d", WEEKDAYS[t->tm_wday], MONTHS[t->tm_mon], t->tm_mday);
  set_label(date_label, text);
  quarter_hour_show(t);
}

void clock_ui_set_weather(const weather_t *weather) {
  if (weather == NULL) {
    lv_obj_add_flag(weather_row, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(weather_row, LV_OBJ_FLAG_HIDDEN);

  if (!icon_valid || icon_condition != weather->condition || icon_is_day != weather->is_day) {
    weather_icon_set(weather_icon, weather->condition, weather->is_day);
    icon_valid = true;
    icon_condition = weather->condition;
    icon_is_day = weather->is_day;
  }

  char text[32];
  set_label(condition_label, weather_condition_name(weather->condition));
  snprintf(text, sizeof(text), "%d°", weather->temperature);
  set_label(temperature_label, text);
  snprintf(text, sizeof(text), "H %d°", weather->high);
  set_label(high_label, text);
  snprintf(text, sizeof(text), "L %d°", weather->low);
  set_label(low_label, text);
}

void clock_ui_set_status(const char *status) {
  set_label(status_label, status);
}
