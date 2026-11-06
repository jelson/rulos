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
// An NTP-synchronized clock, with the weather, on the Waveshare
// ESP32-S3-Touch-LCD-4.3B.

#include <inttypes.h>
#include <stdlib.h>
#include <time.h>

#include "boards/waveshare_esp32_s3_touch_lcd_43b.h"
#include "clock_config.h"
#include "clock_ui.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "periph/ch422g/ch422g.h"
#include "periph/inet/inet.h"
#include "periph/lcd_touch/rulos_lvgl_touch.h"
#include "periph/lvgl/rulos_lvgl.h"
#include "periph/ntp/ntp.h"
#include "periph/openmeteo/openmeteo.h"
#include "periph/uart/uart.h"

// Wi-Fi networks to join; see wifi-credentials-example.h
#include "wifi-credentials.h"

#define JIFFY_CLOCK_US 10000
// how often to look again while there is no time to show
#define UI_REFRESH_PERIOD_US 100000
// how far ahead of a second's start its frame is prepared; covers the
// scheduler's wake-up slop and the drawing
#define SECOND_LEAD_US 25000
// the longest a refresh waits for a second's start; woken later than this
// before the start, it shows the second under way at once instead
#define SECOND_WAIT_MAX_US 80000
// a second shown later than this past its start gets logged
#define SECOND_LATE_LOG_US      2000
#define WEATHER_HTTP_TIMEOUT_MS 10000
#define WEATHER_REPLY_LEN       1024

// The flash partition holding the cat's animation frames (cat_frames.bin
// from the build), named in the SConstruct's partition table.
#define CAT_FRAMES_PARTITION "cats"

static NtpClient ntp;

//////////////// weather /////////////////////

// Fetches the weather periodically and shows it. The HTTP client's
// completion arrives on the scheduler thread, so the UI is touched from
// one thread only.
class WeatherFetcher : public HttpsHandlerIfc {
 public:
  WeatherFetcher() : _client(WEATHER_HTTP_TIMEOUT_MS, /*cert=*/NULL) {
    openmeteo_url(_url, sizeof(_url), CLOCK_LATITUDE, CLOCK_LONGITUDE, CLOCK_FAHRENHEIT);
    // one byte is kept back for the terminator
    _client.set_response_buffer(_reply, sizeof(_reply) - 1);
  }

  void start() {
    schedule_now(_fetch_trampoline, this);
  }

  void on_done(HttpsClient *hc, int response_code, size_t response_len) override {
    _reply[response_len] = '\0';
    weather_t weather;
    const bool ok = response_code == 200 && openmeteo_parse(_reply, &weather);
    if (ok) {
      clock_ui_set_weather(&weather);
      LOG("weather: %s, %d now, %d to %d", weather_condition_name(weather.condition),
          weather.temperature, weather.low, weather.high);
    } else {
      LOG("weather: fetch failed, status %d, %u bytes", response_code, (unsigned)response_len);
    }
    _schedule_fetch(ok ? CLOCK_WEATHER_PERIOD_SEC : CLOCK_WEATHER_RETRY_SEC);
  }

 private:
  HttpsClient _client;
  char _url[OPENMETEO_URL_LEN];
  char _reply[WEATHER_REPLY_LEN];

  void _schedule_fetch(uint32_t delay_sec) {
    schedule_us(delay_sec * 1000000, _fetch_trampoline, this);
  }

  void _fetch() {
    if (!inet_wifi_is_connected()) {
      _schedule_fetch(CLOCK_WEATHER_RETRY_SEC);
      return;
    }
    _client.get(_url, this);
  }

  static void _fetch_trampoline(void *data) {
    static_cast<WeatherFetcher *>(data)->_fetch();
  }
};

static WeatherFetcher weather_fetcher;

//////////////// time /////////////////////

// The seconds digits flip with the second. Each refresh wakes a little
// ahead of a second's start by the NTP client's clock, draws that second
// into the off-screen buffer, spins until the second starts, and only
// then lets the panel switch to it.
static void refresh_ui(void *data) {
  if (!inet_wifi_is_connected()) {
    clock_ui_set_time(NULL);
    clock_ui_set_status("Connecting to Wi-Fi");
    schedule_us(UI_REFRESH_PERIOD_US, refresh_ui, NULL);
    return;
  }
  if (!ntp.is_synced()) {
    clock_ui_set_time(NULL);
    clock_ui_set_status("Waiting for NTP");
    schedule_us(UI_REFRESH_PERIOD_US, refresh_ui, NULL);
    return;
  }

  // the second about to start, or the one under way if the wake-up was late
  uint64_t now_usec = ntp.get_epoch_time_usec();
  const uint32_t into_second = now_usec % 1000000;
  const bool ahead = into_second >= 1000000 - SECOND_WAIT_MAX_US;
  const time_t shown = now_usec / 1000000 + (ahead ? 1 : 0);
  const uint64_t start_usec = (uint64_t)shown * 1000000;
  struct tm local;
  localtime_r(&shown, &local);
  clock_ui_set_time(&local);
  clock_ui_set_status("NTP synchronized");
  rulos_lvgl_hold();
  lv_refr_now(NULL);
  while ((now_usec = ntp.get_epoch_time_usec()) < start_usec) {
  }
  rulos_lvgl_release();
  if (now_usec - start_usec > SECOND_LATE_LOG_US) {
    LOG("second shown %" PRIu32 " us late", (uint32_t)(now_usec - start_usec));
  }
  schedule_us(start_usec + 1000000 - SECOND_LEAD_US - now_usec, refresh_ui, NULL);
}

//////////////// main /////////////////////

// The cat's frames are read out of their partition a frame at a time. They
// are not memory-mapped: with any flash page mapped, every flash write
// (Wi-Fi settles into NVS at startup) suspends the cache, and the panel's
// bounce-buffer interrupt faults copying from the PSRAM frame buffer.
static void read_cat_frames(void *ctx, uint32_t offset, uint32_t size, uint8_t *dst) {
  ESP_ERROR_CHECK(esp_partition_read((const esp_partition_t *)ctx, offset, dst, size));
}

static sprite_blob_t cat_frames_blob() {
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, CAT_FRAMES_PARTITION);
  assert(partition != NULL);
  sprite_blob_t blob = {read_cat_frames, (void *)partition};
  return blob;
}

// Brings up the GT911 touch controller. It picks its I2C address from
// its interrupt pin while it comes out of reset: held low here, so the
// address is the driver's default.
static esp_lcd_touch_handle_t touch_init(i2c_master_bus_handle_t bus, ch422g_t *expander) {
  gpio_make_output(WAVESHARE_43B_TOUCH_IRQ);
  gpio_clr(WAVESHARE_43B_TOUCH_IRQ);
  ch422g_set_exio(expander, WAVESHARE_43B_EXIO_TP_RST, false);
  vTaskDelay(pdMS_TO_TICKS(10));
  ch422g_set_exio(expander, WAVESHARE_43B_EXIO_TP_RST, true);
  vTaskDelay(pdMS_TO_TICKS(50));

  esp_lcd_panel_io_i2c_config_t io_config = {};
  io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
  io_config.scl_speed_hz = 400 * 1000;
  io_config.control_phase_bytes = 1;
  io_config.dc_bit_offset = 0;
  io_config.lcd_cmd_bits = 16;
  io_config.flags.disable_control_phase = 1;
  esp_lcd_panel_io_handle_t io;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(bus, &io_config, &io));

  esp_lcd_touch_config_t config = {};
  config.x_max = WAVESHARE_43B_LCD_H_RES;
  config.y_max = WAVESHARE_43B_LCD_V_RES;
  config.rst_gpio_num = GPIO_NUM_NC;
  config.int_gpio_num = (gpio_num_t)WAVESHARE_43B_TOUCH_IRQ;
  esp_lcd_touch_handle_t touch;
  ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io, &config, &touch));
  return touch;
}

static i2c_master_bus_handle_t i2c_bus_init() {
  i2c_master_bus_config_t config = {};
  config.i2c_port = I2C_NUM_0;
  config.sda_io_num = (gpio_num_t)WAVESHARE_43B_I2C_SDA;
  config.scl_io_num = (gpio_num_t)WAVESHARE_43B_I2C_SCL;
  config.clk_source = I2C_CLK_SRC_DEFAULT;
  config.glitch_ignore_cnt = 7;
  config.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t bus;
  ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus));
  return bus;
}

int main() {
  rulos_hal_init();

  UartState_t uart;
  uart_init(&uart, UART_ID_USB_SERIAL_JTAG, 1000000);
  log_bind_uart(&uart);
  LOG("ntpclock starting");

  init_clock(JIFFY_CLOCK_US, TIMER0);
  setenv("TZ", CLOCK_TZ, 1);
  tzset();

  // display: panel first, then the backlight once there is a frame to show
  rulos_lvgl_init();
  const esp_lcd_rgb_panel_config_t panel_config = waveshare_43b_rgb_panel_config();
  lv_display_t *display = rulos_lvgl_add_rgb_panel(&panel_config);
  const sprite_blob_t cat_frames = cat_frames_blob();
  clock_ui_create(&cat_frames);
  i2c_master_bus_handle_t i2c_bus = i2c_bus_init();
  static ch422g_t expander;
  ch422g_init(&expander, i2c_bus);
  ch422g_set_exio(&expander, WAVESHARE_43B_EXIO_LCD_BACKLIGHT, true);
  rulos_lvgl_add_touch(display, touch_init(i2c_bus, &expander));

  inet_wifi_client_start(wifi_creds, sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  ntp.start();
  weather_fetcher.start();

  schedule_now(refresh_ui, NULL);
  scheduler_run();
}
