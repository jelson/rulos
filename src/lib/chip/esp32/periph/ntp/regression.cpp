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

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <functional>

#include "periph/ntp/ntp.h"

#define logappend(...)                                     \
  do {                                                     \
    int len = snprintf(logbuf, *logbufcap, ##__VA_ARGS__); \
    logbuf += len;                                         \
    *logbufcap -= len;                                     \
  } while (0)

// This is not part of the NtpClient class so it can be compiled
// separately as part of a unit test that's compiled natively on an
// x64 host
bool update_epoch_estimate(const time_observation_t *obs, char *logbuf,
                           int *logbufcap, uint64_t *offset_usec /* OUT */,
                           int64_t *freq_ppb /* OUT */) {
  // Make a sorted list of RTTs so we can compute quartile statistics.
  // Also, find the minimum epoch time of all observations.
  uint32_t rtts[NtpClient::MAX_OBSERVATIONS];
  int num_obs = 0;
  uint64_t min_epoch = obs[0].epoch_time_usec;

  logappend("rtts_now:");
  for (int i = 0; i < NtpClient::MAX_OBSERVATIONS; i++) {
    if (obs[i].local_time_usec == 0) {
      continue;
    }

    rtts[num_obs] = obs[i].rtt_usec;
    logappend("%u,", rtts[num_obs]);
    num_obs++;

    if (obs[i].epoch_time_usec < min_epoch) {
      min_epoch = obs[i].epoch_time_usec;
    }
  }

  // If we don't have enough observations yet, stop
  if (num_obs < NtpClient::MIN_OBSERVATIONS) {
    logappend("insufficient_obs");
    return false;
  }

  // Sort the list of RTTs
  std::sort(rtts, rtts + num_obs);
  logappend("rtts_sorted:");
  for (int i = 0; i < num_obs; i++) {
    logappend("%u,", rtts[i]);
  }

  // Select the max rtt we're willing to accept; throw away the top
  // quartile
  int rtt_limit_idx = (num_obs * 3 / 4) - 1;
  uint32_t rtt_limit = rtts[rtt_limit_idx];
  logappend("rtt_limit_idx=%d,rtt_limit=%u,", rtt_limit_idx, rtt_limit);

#if SIMULATOR
  printf("[\n");
#endif
  // Compute subterms used in the linear regression. Subtract off the
  // minimum epoch time from all y values so we get more bits of free
  // precision.
  double sumX = 0, sumX2 = 0, sumY = 0, sumXY = 0;
  int n = 0;
  for (int i = 0; i < NtpClient::MAX_OBSERVATIONS; i++) {
    const time_observation_t *to = &obs[i];
    if (to->rtt_usec == 0 || to->rtt_usec > rtt_limit) {
      continue;
    }
    double x = to->local_time_usec;
    double y = to->epoch_time_usec - min_epoch;
#if SIMULATOR
    printf("[ %9u , %12.12g , %13lu , %12.12g ]%s\n", to->rtt_usec, x,
           to->epoch_time_usec, y,
           i == NtpClient::MAX_OBSERVATIONS - 1 ? "" : ",");
#endif
    sumX += x;
    sumX2 += x * x;
    sumY += y;
    sumXY += x * y;
    n++;
  }
#if SIMULATOR
  printf("]\n");
#endif

  // Compute the linear regression
  double f_m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
  double f_b_shifted = (sumY - f_m * sumX) / n;
  logappend("shifted_offset_usec=%f,slope=%f,", f_b_shifted, f_m);

  // Convert slope into a frequency error in parts-per-billion
  double f_ppb = (1.0 - f_m) * 1e9;
  logappend("ppb=%f,", f_ppb);

  // Convert the floats back into 64-bit integers. Add back in the
  // offset we subtracted out earlier.
  *offset_usec = ((uint64_t)round(f_b_shifted)) + min_epoch;
  *freq_ppb = (int64_t)round(f_ppb);
  return true;
}

uint64_t local_to_epoch(uint64_t local_time_usec, uint64_t offset_usec,
                        int64_t freq_ppb) {
  // compute the base offset
  uint64_t epoch_usec = local_time_usec + offset_usec;

  // add in correction for frequency error. The equation we want is:
  //
  // epoch_usec += (local_time_usec * freq_ppb) / 1e9;
  //
  // But we split up the division in order to prevent overflow for
  // long time intervals. If the freq error is 1M PPB (1,000 PPM), the
  // naive equation above would overflow the 64-bit intermediate value
  // after 213 days, which is a little on the short side. So instead
  // we first divide local time by 1,000, then multiply by error, then
  // divide by the remaining million.
  int64_t freq_correction = local_time_usec / 1e3;
  freq_correction *= freq_ppb;
  freq_correction /= 1e6;
  epoch_usec -= freq_correction;

  return epoch_usec;
}
