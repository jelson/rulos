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

#include <stdbool.h>
#include <string.h>

#include "periph/nvconfig/nvconfig.h"
#include "pulsegen.h"

// Settings change in RAM; only CONFig:SAVE and *RST update flash. Keep the persisted payload
// layout stable and bump its version on layout changes.
#define PG_CFG_VERSION 1

typedef struct {
  uint64_t period_ps;
  uint64_t width_ps;
  uint64_t delay_ps;
  uint64_t burst_rep_ps;
  uint32_t burst_ncyc;
  uint8_t on;
  uint8_t burst_on;
} pg_persist_chan_t;

typedef struct {
  uint8_t sync;
  pg_persist_chan_t chan[NUM_CHANNELS];
} pg_persist_t;

// Cache only configurations known to be in flash, so a failed save remains retryable.
static pg_persist_t cfg_last_saved;
static bool cfg_last_saved_valid;

static void cfg_snapshot(pg_persist_t *c) {
  memset(c, 0, sizeof(*c));
  c->sync = pulsegen_get_mode_sync();
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pg_persist_chan_t *ch = &c->chan[i];
    ch->period_ps = pulsegen_get_period_ps(i);
    ch->width_ps = pulsegen_get_width_ps(i);
    ch->delay_ps = pulsegen_get_delay_ps(i);
    ch->burst_rep_ps = pulsegen_get_burst_period_ps(i);
    ch->burst_ncyc = pulsegen_get_burst_ncyc(i);
    ch->on = pulsegen_get_state(i);
    ch->burst_on = pulsegen_get_burst_state(i);
  }
}

// Restore through the public setters, in dependency order: period before width and delay (which
// are validated against it), burst parameters before burst state, and outputs last so a channel
// only turns on once its whole configuration is in place.
static void cfg_apply(const pg_persist_t *c) {
  pulsegen_set_mode(c->sync);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    const pg_persist_chan_t *ch = &c->chan[i];
    pulsegen_set_period_ps(i, ch->period_ps);
    pulsegen_set_width_ps(i, ch->width_ps);
    pulsegen_set_delay_ps(i, ch->delay_ps);
    pulsegen_set_burst_ncyc(i, ch->burst_ncyc);
    pulsegen_set_burst_period_ps(i, ch->burst_rep_ps);
    pulsegen_set_burst_state(i, ch->burst_on);
  }
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pulsegen_set_state(i, c->chan[i].on);
  }
}

void pulsegen_config_load(void) {
  pg_persist_t saved;
  cfg_last_saved_valid = nvconfig_load(&saved, sizeof(saved), PG_CFG_VERSION);
  pulsegen_apply_defaults();
  if (cfg_last_saved_valid) {
    cfg_apply(&saved);
    cfg_last_saved = saved;
  }
}

bool pulsegen_config_save(void) {
  pg_persist_t c;
  cfg_snapshot(&c);
  if (cfg_last_saved_valid && memcmp(&c, &cfg_last_saved, sizeof(c)) == 0) {
    return true;
  }
  // The pulse timers run from hardware, so outputs continue through the erase and program. The
  // core itself stalls on flash-busy for those tens of milliseconds (the region shares bank 1
  // with the code), which only delays command handling and any burst re-arm due in the window.
  bool saved = nvconfig_save(&c, sizeof(c), PG_CFG_VERSION);
  if (saved) {
    cfg_last_saved = c;
  }
  // A failure can occur after the new slot was programmed (for example, locking flash). Its
  // contents are then uncertain, so even reverting to the previously saved config must write.
  cfg_last_saved_valid = saved;
  return saved;
}

bool pulsegen_reset_all(void) {
  pulsegen_apply_defaults();
  return pulsegen_config_save();
}
