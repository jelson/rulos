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

#include "periph/ntp/ntp.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/rulos.h"
#include "core/wallclock.h"
#include "esp_wifi.h"
#include "periph/ntp/ntp-packet.h"

extern "C" {
#include <errno.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
}

uint64_t NtpClient::_resp_time_usec;
wallclock_t NtpClient::_uptime;

bool NtpClient::_sendRequest(ntp_packet_t *req) {
  // ensure there isn't already a transaction outstanding
  if (_req_time_usec != 0) {
    LOG("[NTP] not sending req; one is outstanding already");
    return false;
  }

  // resolve hostname
  struct hostent *server;
  server = gethostbyname(_hostname);
  if (server == NULL) {
    LOG("[NTP] could not resolve '%s': errno %d", _hostname, errno);
    return false;
  }
  uint32_t remote_ip;
  if (server->h_addrtype != AF_INET) {
    LOG("[NTP] got weird addr type of %d for host %s", server->h_addrtype,
        _hostname);
    return false;
  }

  // log and set destination address
  memcpy(&remote_ip, server->h_addr_list[0], sizeof(remote_ip));
  struct in_addr a;
  a.s_addr = remote_ip;
  LOG("[NTP] resolved %s to %s", _hostname, inet_ntoa(a));
  struct sockaddr_in dest;
  dest.sin_addr.s_addr = remote_ip;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(NTP_PORT);

  // create socket, if not already created
  if (_sock == -1) {
    if ((_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      LOG("[NTP] could not create socket: %d", errno);
      return false;
    }

    fcntl(_sock, F_SETFL, O_NONBLOCK);
  }

  // clear the incoming buffer in case of duplicate packet receptions
  int len = 0;
  do {
    char resp[100];
    len = recvfrom(_sock, &resp, sizeof(resp), MSG_DONTWAIT, NULL, NULL);
  } while (len > 0);

  LOG("[NTP] sending request");
  esp_wifi_set_promiscuous(true);
  _resp_time_usec = 0;
  _req_time_usec = wallclock_get_uptime_usec(&_uptime);
  int sent = sendto(_sock, req, sizeof(*req), 0, (struct sockaddr *)&dest,
                    sizeof(dest));
  if (sent < 0) {
    LOG("[NTP] could not send UDP packet to %s: errno %d", _hostname, errno);
    esp_wifi_set_promiscuous(false);
    _req_time_usec = 0;
    return false;
  }

  return true;
}

void NtpClient::_schedule_next_sync() {
  Time period;

  if (is_synced()) {
    period = SYNCED_OBSERVATION_PERIOD_SEC;
  } else {
    period = NOTSYNCED_OBSERVATION_PERIOD_SEC;
  }
  schedule_us(period * 1e6, NtpClient::_sync_trampoline, this);
}

void NtpClient::_sync() {
  _schedule_next_sync();

  ntp_packet_t req;
  memset(&req, 0, sizeof(req));
  req.version = 3;
  req.mode = 3;

  bool ok = _sendRequest(&req);
  if (!ok) {
    LOG("[NTP] request failed");
    return;
  }

  // request succeeded! schedule an attempt at reception
  schedule_us(0, NtpClient::_try_receive_trampoline, this);
}

void NtpClient::_sync_trampoline(void *data) {
  NtpClient *ntp = static_cast<NtpClient *>(data);
  ntp->_sync();
}

void NtpClient::sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) {
    return;
  }
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t *)buf;
  // this is cheesy; in theory we should parse the whole packet and
  // make sure it's UDP, destined for our port number. but this
  // quick-and-dirty check for the right length is a quick hack which
  // works since sniffing is only turned on after we send a request
  if (p->rx_ctrl.sig_len != 130) {
    return;
  }
  NtpClient::_resp_time_usec = wallclock_get_uptime_usec(&NtpClient::_uptime);
}

void NtpClient::_try_receive() {
  ntp_packet_t resp;
  int len = recvfrom(_sock, &resp, sizeof(resp), MSG_DONTWAIT, NULL, NULL);

  if (len == -1 && errno == EWOULDBLOCK) {
    // no data available yet.  how long have we been waiting for a
    // response?
    uint64_t now = wallclock_get_uptime_usec(&_uptime);
    uint32_t rtt_usec = now - _req_time_usec;

    if (rtt_usec > NTP_TIMEOUT_US) {
      LOG("[NTP] Timeout waiting for response");
      len = -1;
    } else {
      schedule_us(0, NtpClient::_try_receive_trampoline, this);
      return;
    }
  }

  // request no longer outstanding
  esp_wifi_set_promiscuous(false);
  uint32_t rtt_usec = _resp_time_usec - _req_time_usec;
  _req_time_usec = 0;

  // was resp time recorded?
  if (_resp_time_usec == 0) {
    LOG("[NTP] did not get sniffer timestamp on response");
    return;
  }

  // error on reception? stop here
  if (len == -1) {
    LOG("[NTP] could not receive response: %d", errno);
    return;
  }

  // check for valid response length
  if (len != sizeof(ntp_packet_t)) {
    LOG("[NTP] got weird %d-byte response from ntp server", len);
    return;
  }

  // packet successfully received!
  LOG("[NTP] got response after %u usec", rtt_usec);

  // time at which the ntp server transmitted its response
  uint64_t server_ntp_usec =
      (((uint64_t)1000000) * ntohl(resp.txTime_frac)) >> 32;
  server_ntp_usec += (uint64_t)1000000 * ntohl(resp.txTime_sec);

  // how long the remote server took between its incoming and outgoing
  // packets
  uint64_t rx_at_server_ntp_usec =
      (((uint64_t)1000000) * ntohl(resp.rxTime_frac)) >> 32;
  rx_at_server_ntp_usec += (uint64_t)1000000 * ntohl(resp.rxTime_sec);
  uint64_t server_delay_usec = server_ntp_usec - rx_at_server_ntp_usec;

  // estimate one way latency: half of (locally observed RTT - server delay)
  uint32_t oneway_latency_usec = (rtt_usec - server_delay_usec) / 2;

  // compute the epoch time
  uint64_t server_epoch_usec =
      server_ntp_usec - ((uint64_t)1000000 * UNIX_EPOCH_NTP_TIMESTAMP);

  // our estimate of the true epoch time when this packet was received locally
  uint64_t epoch_time_when_received = server_epoch_usec + oneway_latency_usec;

  // add in empirically derived correction for the latency asymmetry in the send
  // and receive directions of the esp32's wifi stack
  epoch_time_when_received -= SEND_TIME_BIAS_USEC;

  uint64_t offset_usec = epoch_time_when_received - _resp_time_usec;

  // record this observation in the circlular buffer
  time_observation_t *this_obs = &_obs[_obs_idx];
  _obs_idx++;
  if (_obs_idx >= MAX_OBSERVATIONS) {
    _obs_idx = 0;
  }
  this_obs->local_time_usec = _resp_time_usec;
  this_obs->epoch_time_usec = epoch_time_when_received;
  this_obs->rtt_usec = rtt_usec;

  // log statistics
  char logbuf[1000];
  int logcapacity = sizeof(logbuf);
  logcapacity -= snprintf(
      logbuf, sizeof(logbuf),
      "[NTP] stats: "
      "sent=%llu,local_rx_time=%llu,server_delay=%llu,server_epoch_usec=%llu,"
      "raw_rtt_usec=%u,oneway_latency_usec=%u,epoch_time_when_received=%llu,"
      "this_offset_usec=%lld,",
      _req_time_usec, _resp_time_usec, server_delay_usec, server_epoch_usec,
      rtt_usec, oneway_latency_usec, epoch_time_when_received, offset_usec);

  _locked =
      update_epoch_estimate(_obs, &logbuf[sizeof(logbuf) - logcapacity],
                            &logcapacity, &_offset_usec, &_freq_error_ppb);
  logbuf[sizeof(logbuf) - logcapacity] = '\n';
  logcapacity--;
  log_write(logbuf, sizeof(logbuf) - logcapacity);
  _req_time_usec = 0;
}

void NtpClient::_try_receive_trampoline(void *data) {
  NtpClient *ntp = static_cast<NtpClient *>(data);
  ntp->_try_receive();
}

bool NtpClient::is_synced(void) {
  return _locked;
}

void NtpClient::get_epoch_and_local_usec(uint64_t *epoch, uint64_t *local) {
  if (is_synced()) {
    *local = wallclock_get_uptime_usec(&_uptime);
    *epoch = local_to_epoch(*local, _offset_usec, _freq_error_ppb);
  } else {
    *local = 0;
    *epoch = 0;
  }
}

uint32_t NtpClient::get_epoch_time_sec(void) {
  return get_epoch_time_usec() / 1000000;
}

uint64_t NtpClient::get_epoch_time_usec(void) {
  uint64_t epoch, local;
  get_epoch_and_local_usec(&epoch, &local);
  return epoch;
}

void NtpClient::start(void) {
  wallclock_init(&_uptime);

  const wifi_promiscuous_filter_t filt = {.filter_mask =
                                              WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  schedule_now(NtpClient::_sync_trampoline, this);
}

void NtpClient::_init(const char *hostname) {
  _sock = -1;
  _hostname = hostname;
  memset(&_obs, 0, sizeof(_obs));
  _obs_idx = 0;
}

NtpClient::NtpClient() {
#ifdef RULOS_NTP_SERVER
  _init(RULOS_NTP_SERVER);
#else
  _init(DEFAULT_SERVER);
#endif
}

NtpClient::NtpClient(const char *hostname) {
  _init(hostname);
}
