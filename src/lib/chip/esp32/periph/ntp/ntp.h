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
#include "esp_wifi.h"
#include "periph/ntp/ntp-packet.h"
#include "periph/ntp/regression.h"

class NtpClient {
 public:
  const char *DEFAULT_SERVER = "us.pool.ntp.org";
  static const uint16_t NTP_PORT = 123;
  static const int32_t NTP_TIMEOUT_US = 2000000;
  static const uint16_t NOTSYNCED_OBSERVATION_PERIOD_SEC = 4;
  static const uint16_t PARTIAL_SYNC_OBSERVATION_PERIOD_SEC = 20;
  static const uint16_t SYNCED_OBSERVATION_PERIOD_SEC = 60;
  static const uint32_t SEND_TIME_BIAS_USEC = 200;

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
  static uint64_t _resp_time_usec;
  static wallclock_t _uptime;

  bool _locked;
  int64_t _most_recent_offset_usec;
  // circular buffer of recent observations
  time_observation_t _obs[MAX_OBSERVATIONS];
  // index of last obs written
  uint16_t _obs_idx;

  // linear regression outputs
  uint64_t _offset_usec;
  int64_t _freq_error_ppb;

  void _init(const char *hostname);
  void _schedule_next_sync();
  void _sync();
  static void _sync_trampoline(void *data);
  bool _sendRequest(ntp_packet_t *req);
  void _try_receive();
  static void sniffer(void *buf, wifi_promiscuous_pkt_type_t type);
  static void _try_receive_trampoline(void *data);
  int _update_epoch_estimate(char *logbuf, int logbuflen);
};
