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

#include <string.h>

#include "core/hardware.h"
#include "periph/nvconfig/nvconfig.h"
#include "timestamper.h"

// Channel settings change in RAM; only CONFig:SAVE and *RST update flash. Keep the persisted
// payload layout stable across this module's refactoring and bump its version on layout changes.
#define TS_CFG_VERSION 1

typedef struct {
  uint8_t slope[NUM_CHANNELS];
  uint32_t divider[NUM_CHANNELS];
} ts_persist_t;

// Cache only configurations known to be in flash, so a failed save remains retryable.
static ts_persist_t cfg_last_saved;
static bool cfg_last_saved_valid;

static void cfg_snapshot(ts_persist_t *c) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    c->slope[i] = (uint8_t)timestamper_get_slope(i);
    c->divider[i] = timestamper_get_divider(i);
  }
}

void timestamper_config_load(void) {
  ts_persist_t saved;
  cfg_last_saved_valid = nvconfig_load(&saved, sizeof(saved), TS_CFG_VERSION);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    timestamper_set_slope(
        i, cfg_last_saved_valid ? (timestamper_slope_t)saved.slope[i] : TIMESTAMPER_SLOPE_RISING);
    timestamper_set_divider(i, cfg_last_saved_valid && saved.divider[i] ? saved.divider[i] : 1);
  }
  if (cfg_last_saved_valid) {
    cfg_last_saved = saved;
  }
}

bool timestamper_config_save(void) {
  ts_persist_t c;
  cfg_snapshot(&c);
  if (cfg_last_saved_valid && memcmp(&c, &cfg_last_saved, sizeof(c)) == 0) {
    return true;
  }
  // Silence capture/stream ISRs across erase/program; pulses arriving during a save are lost by
  // design. Preserve a caller's existing interrupt mask.
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  bool saved = nvconfig_save(&c, sizeof(c), TS_CFG_VERSION);
  if (!primask) {
    __enable_irq();
  }
  if (saved) {
    cfg_last_saved = c;
  }
  // A failure can occur after the new slot was programmed (for example, locking flash). Its
  // contents are then uncertain, so even reverting to the previously saved config must write.
  cfg_last_saved_valid = saved;
  return saved;
}

bool timestamper_reset_all(void) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    timestamper_set_slope(i, TIMESTAMPER_SLOPE_RISING);
    timestamper_set_divider(i, 1);
  }
  timestamper_set_stream_enabled(true);
  timestamper_set_format(TIMESTAMPER_FORMAT_TEXT);
  timestamper_serial_set_enabled(false);
  timestamper_serial_set_baud(SERIAL_DEFAULT_BAUD);
  timestamper_discard_pending();
  return timestamper_config_save();
}
