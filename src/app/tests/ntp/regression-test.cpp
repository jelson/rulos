#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "periph/ntp/ntp.h"

int rand_range(int minval, int maxval) {
  int range = maxval - minval + 1;
  return (random() % range) + minval;
}

void test_rand() {
  printf("rand test\n");

  const int minval = -10;
  const int maxval = 10;

  bool seen_min = false, seen_max = false;

  for (int i = 0; i < 1000; i++) {
    int val = rand_range(minval, maxval);
    assert(val >= minval);
    assert(val <= maxval);
    if (val == minval) {
      seen_min = true;
    }
    if (val == maxval) {
      seen_max = true;
    }
  }
  assert(seen_min);
  assert(seen_max);
}

static const time_observation_t real_data_1[NtpClient::MAX_OBSERVATIONS] = {
    {
        .local_time_usec = 15132128,
        .epoch_time_usec = 1643967455758940,
        .rtt_usec = 103972,
    },
    {
        .local_time_usec = 30006036,
        .epoch_time_usec = 1643967470584399,
        .rtt_usec = 5311,
    },
    {
        .local_time_usec = 45005772,
        .epoch_time_usec = 1643967485583909,
        .rtt_usec = 5165,
    },
    {
        .local_time_usec = 60005010,
        .epoch_time_usec = 1643967500582624,
        .rtt_usec = 4513,
    },
};

void regress(const time_observation_t *obs, uint64_t *offset_usec,
             int64_t *freq_ppb) {
  char buf[1000];
  int bufcap = sizeof(buf);
  update_epoch_estimate(obs, buf, &bufcap, offset_usec, freq_ppb);
  buf[sizeof(buf) - bufcap] = '\0';
  printf("Output: %soffset=%lu,freq=%ld\n", buf, *offset_usec, *freq_ppb);
}

void test_perfect_clocks() {
  printf("perfect clocks test\n");
  // Simplest case: the epoch and local clocks are ticking at exactly
  // the same rates, and we can observe the remote clock with no error.

  uint64_t start_local = 10000;
  uint64_t start_epoch = 1643967455758940;

  time_observation_t o[NtpClient::MAX_OBSERVATIONS] = {};
  for (int i = 0; i < 10; i++) {
    o[i].rtt_usec = 1000;
    o[i].local_time_usec = start_local + (i * 15 * 1e6);
    o[i].epoch_time_usec = start_epoch + (i * 15 * 1e6);
  }
  uint64_t offset_usec;
  int64_t freq_ppb;
  regress(o, &offset_usec, &freq_ppb);

  // ensure the analyzer told us the frequency error was 0, and the
  // offset is correct
  assert(freq_ppb == 0);
  assert(offset_usec == start_epoch - start_local);

  // try to convert some timestamps
  for (int i = 0; i < 100; i++) {
    uint64_t local_time_sec = i * 10;
    uint64_t local_time_usec = local_time_sec * 1e6;
    uint64_t expected_epoch_time = local_time_usec + start_epoch - start_local;
    assert(expected_epoch_time ==
           local_to_epoch(local_time_usec, offset_usec, freq_ppb));
  }

  printf("PASS\n");
}

void test_clock_rate_error() {
  printf("clock rate error test\n");
  // Test the case that the local clock and epoch clocks are ticking
  // at different rates, but we can still observe the remote clock
  // with no error.

  uint64_t start_local = 1500000;
  uint64_t start_epoch = 1643967455123456;
  int32_t freq_error_ppm = -250;

  time_observation_t o[NtpClient::MAX_OBSERVATIONS] = {};
  for (int i = 0; i < 10; i++) {
    o[i].rtt_usec = 1000;
    o[i].local_time_usec = start_local + (i * 10 * 1e6);
    o[i].epoch_time_usec = start_epoch + (i * 10) * (1e6 - freq_error_ppm);
  }
  uint64_t offset_usec;
  int64_t freq_ppb;
  regress(o, &offset_usec, &freq_ppb);

  // make sure freq error was computed correctly
  assert(freq_ppb == freq_error_ppm * 1000);

  // do some test conversions
  for (int i = 0; i < 10; i++) {
    assert(o[i].epoch_time_usec ==
           local_to_epoch(o[i].local_time_usec, offset_usec, freq_ppb));
  }

  printf("PASS\n");
}

void test_with_observation_error() {
  printf("observation error test\n");
  // Test the case that the local clock and epoch clocks are ticking
  // at different rates, and there's random (but, gaussian, unbiased)
  // error in each observation

  uint64_t start_local = 1500000;
  uint64_t start_epoch = 1643967455123456;
  uint64_t sec_between_probes = 10000;

  // do the entire test 10 times
  for (int testnum = 0; testnum < 50; testnum++) {
    int32_t freq_error_ppm = rand_range(-1000, 1000);
    printf("test %d: picked error of %d ppm\n", testnum, freq_error_ppm);

    time_observation_t o[NtpClient::MAX_OBSERVATIONS] = {};
    uint32_t rtt = rand_range(1000, 50000);
    for (int i = 0; i < NtpClient::MAX_OBSERVATIONS; i++) {
      o[i].rtt_usec = rtt;
      o[i].local_time_usec = i * sec_between_probes * 1e6;
      o[i].epoch_time_usec = start_epoch +
        (i * sec_between_probes) * (1e6 - freq_error_ppm) + rand_range(-rtt, rtt);
    }
    uint64_t offset_usec;
    int64_t freq_ppb;
    regress(o, &offset_usec, &freq_ppb);

    // make sure freq error was computed within 10ppm
    assert(abs(freq_ppb - freq_error_ppm * 1000) < 10000);

    // do some test conversions; make sure each is accurate to within 1ms
    for (int i = 0; i < 10; i++) {
      uint64_t expected = start_epoch + (i * sec_between_probes) * (1e6 - freq_error_ppm);
      uint64_t actual = local_to_epoch(o[i].local_time_usec, offset_usec, freq_ppb);
      printf("x=%lu,expected=%lu,actual=%lu,error=%ld\n", o[i].local_time_usec,
             expected, actual, expected-actual);
      assert(llabs(expected-actual) < rtt);
    }
    printf("test %d: passed\n", testnum);
  }
  printf("PASS\n");
}

void test_real_data() {
  uint64_t offset_usec;
  int64_t freq_ppb;
  regress(real_data_1, &offset_usec, &freq_ppb);
}

int main(int argc, char *argv[]) {
  srandom(time(0));
  test_rand();
  test_perfect_clocks();
  test_clock_rate_error();
  test_with_observation_error();
  //test_real_data();
}
