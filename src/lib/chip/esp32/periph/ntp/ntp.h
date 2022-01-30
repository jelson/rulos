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
#include "periph/ntp/ntp-packet.h"

class NtpClient {
 public:
  const char *DEFAULT_SERVER = "us.pool.ntp.org";
  const uint16_t NTP_PORT = 123;
  const int32_t NTP_TIMEOUT_US = 2000000;

  NtpClient(void);
  NtpClient(const char *hostname);

  void start(void);
  bool is_synced(void);
  uint32_t get_epoch_time_sec(void);
  uint64_t get_epoch_time_msec(void);

 private:
  const char *_hostname;
  int _sock;
  Time _req_time;

  void _init(const char *hostname);
  void _schedule_next_sync();
  void _sync();
  static void _sync_trampoline(void *data);
  bool _sendRequest(ntp_packet_t *req);
  void _try_receive();
  static void _try_receive_trampoline(void *data);
};
