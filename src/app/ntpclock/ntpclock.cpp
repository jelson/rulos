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

#include <stdlib.h>
#include <time.h>

#include "boards/waveshare_esp32_s3_touch_lcd_43b.h"
#include "clock_config.h"
#include "clock_ui.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "driver/i2c_master.h"
#include "periph/ch422g/ch422g.h"
#include "periph/inet/inet.h"
#include "periph/lvgl/rulos_lvgl.h"
#include "periph/ntp/ntp.h"
#include "periph/openmeteo/openmeteo.h"
#include "periph/uart/uart.h"

// Wi-Fi networks to join; see wifi-credentials-example.h
#include "wifi-credentials.h"

#define JIFFY_CLOCK_US          10000
#define UI_REFRESH_PERIOD_US    100000
#define WEATHER_HTTP_TIMEOUT_MS 10000
#define WEATHER_REPLY_LEN       1024

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

static void refresh_ui(void *data) {
  schedule_us(UI_REFRESH_PERIOD_US, refresh_ui, NULL);

  if (!inet_wifi_is_connected()) {
    clock_ui_set_time(NULL);
    clock_ui_set_status("Connecting to Wi-Fi");
    return;
  }
  if (!ntp.is_synced()) {
    clock_ui_set_time(NULL);
    clock_ui_set_status("Waiting for NTP");
    return;
  }

  time_t now = ntp.get_epoch_time_sec();
  struct tm local;
  localtime_r(&now, &local);
  clock_ui_set_time(&local);
  clock_ui_set_status("NTP synchronized");
}

//////////////// main /////////////////////

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
  rulos_lvgl_add_rgb_panel(&panel_config);
  clock_ui_create();
  static ch422g_t expander;
  ch422g_init(&expander, i2c_bus_init());
  ch422g_set_exio(&expander, WAVESHARE_43B_EXIO_LCD_BACKLIGHT, true);

  inet_wifi_client_start(wifi_creds, sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  ntp.start();
  weather_fetcher.start();

  schedule_now(refresh_ui, NULL);
  scheduler_run();
}
