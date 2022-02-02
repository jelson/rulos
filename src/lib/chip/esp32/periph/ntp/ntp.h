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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/rulos.h"
#include "core/wallclock.h"
#include "periph/ntp/ntp-packet.h"

typedef struct {
  Time local_time;
  int64_t epoch_time;
} time_observation_t;

class NtpClient {
 public:
  const char *DEFAULT_SERVER = "us.pool.ntp.org";
  static const uint16_t NTP_PORT = 123;
  static const int32_t NTP_TIMEOUT_US = 2000000;
  static const uint16_t OBSERVATION_PERIOD_SEC = 30;
  static const uint16_t MAX_OBSERVATIONS = 10;

  NtpClient(void);
  NtpClient(const char *hostname);

  void start(void);
  bool is_synced(void);
  uint32_t get_epoch_time_sec(void);
  uint64_t get_epoch_time_usec(void);
  void get_epoch_and_local_usec(uint64_t *epoch, uint64_t *local);

 private:
  const char *_hostname;
  int _sock;
  uint64_t _req_time_usec;
  wallclock_t _uptime;
  int64_t offset_usec;
  time_observation_t obs[MAX_OBSERVATIONS];
  uint16_t obs_idx;

  void _init(const char *hostname);
  void _schedule_next_sync();
  void _sync();
  static void _sync_trampoline(void *data);
  bool _sendRequest(ntp_packet_t *req);
  void _try_receive();
  static void _try_receive_trampoline(void *data);
};
