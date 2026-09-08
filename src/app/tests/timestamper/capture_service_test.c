// Compile the production service/merge against scripted timer and DMA publication registers.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CCMRAM
#define CLOCK_FREQ_HZ 250000000U
#define NUM_SUBS      2
#define SUB_RISING    0
#define SUB_FALLING   1

typedef enum {
  TIMESTAMPER_SLOPE_RISING,
  TIMESTAMPER_SLOPE_FALLING,
  TIMESTAMPER_SLOPE_BOTH,
} timestamper_slope_t;
typedef struct {
  uint32_t seconds, counter;
} timestamp_t;
typedef struct {
  volatile uint32_t CNT, SR;
} TIM_TypeDef;
typedef struct {
  volatile uint32_t *buf;
  uint32_t source, destination;
} rulos_dma_channel_t;
typedef struct {
  struct {
    volatile uint32_t *buf;
    uint32_t drain_pos;
    rulos_dma_channel_t *dma_ch;
    uint32_t counter_tag;
    bool discard_before_epoch;
  } sub[NUM_SUBS];
  TIM_TypeDef *capture_tim;
  uint32_t capture_if_mask;
  timestamp_t capture_epoch;
  uint32_t buf_overflows, divider, count;
  timestamper_slope_t slope;
  volatile bool recent_pulse;
} channel_t;

static timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head, ts_tail, seconds_A, seconds_B;
static volatile uint32_t captures[NUM_SUBS][DMA_CAPTURE_BUFLEN];
static TIM_TypeDef timer;
static rulos_dma_channel_t dma[NUM_SUBS];
static channel_t channel;
static unsigned accesses, checked;
static void (*access_hook)(unsigned);

static void access_register(void) {
  unsigned current = accesses++;
  if (access_hook) {
    access_hook(current);
  }
}

static uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  access_register();
  return DMA_CAPTURE_BUFLEN - ch->source;
}

static uintptr_t rulos_dma_get_write_address(const rulos_dma_channel_t *ch) {
  access_register();
  return (uintptr_t)ch->buf + ch->destination * sizeof(uint32_t);
}

#include "capture_drain_impl.h"
#include "capture_service_impl.h"

static uint32_t initial_head;

static void reset(uint32_t divider, uint32_t carry, uint32_t start) {
  memset(&channel, 0, sizeof(channel));
  memset(timestamp_buffer, 0, sizeof(timestamp_buffer));
  memset((void *)captures, 0xdd, sizeof(captures));
  timer = (TIM_TypeDef){.CNT = 1000000, .SR = 0};
  seconds_A = seconds_B = 42;
  for (unsigned s = 0; s < NUM_SUBS; s++) {
    dma[s] = (rulos_dma_channel_t){captures[s], start, start};
    channel.sub[s].buf = captures[s];
    channel.sub[s].drain_pos = start;
    channel.sub[s].dma_ch = &dma[s];
    channel.sub[s].counter_tag = (2U << 30) | (s == SUB_RISING ? 1U << 29 : 0);
  }
  channel.capture_tim = &timer;
  channel.capture_if_mask = 3;
  channel.slope = TIMESTAMPER_SLOPE_BOTH;
  channel.divider = divider;
  channel.count = carry;
  ts_head = ts_tail = initial_head = TIMESTAMP_BUFLEN - 3;
  accesses = 0;
  access_hook = NULL;
}

static void publish(unsigned s, uint32_t tick) {
  uint32_t pos = dma[s].destination & (DMA_CAPTURE_BUFLEN - 1);
  captures[s][pos] = tick;
  dma[s].destination = dma[s].source = (pos + 1) & (DMA_CAPTURE_BUFLEN - 1);
}

static unsigned output_count(void) {
  return (ts_head - initial_head) & (TIMESTAMP_BUFLEN - 1);
}

static timestamp_t output(unsigned index) {
  assert(index < output_count());
  return timestamp_buffer[(initial_head + index) & (TIMESTAMP_BUFLEN - 1)];
}

static void expect_output(unsigned index, unsigned s, uint32_t seconds, uint32_t tick) {
  timestamp_t actual = output(index);
  assert(actual.seconds == seconds);
  assert(actual.counter == (tick | channel.sub[s].counter_tag));
}

static void test_delayed_opposite_capture(void) {
  for (unsigned fifo = 0; fifo < 2; fifo++) {
    reset(2, 0, DMA_CAPTURE_BUFLEN - 1);
    publish(SUB_RISING, 300);
    if (fifo) {
      dma[SUB_FALLING].source = 0;  // Old fall fetched, not written; CCIF already cleared.
    } else {
      timer.SR = 2;  // Old fall still in CCR, so source and destination positions agree.
    }
    service_channel(&channel, false);
    assert(channel.count == 0 && output_count() == 0);
    assert(channel.sub[SUB_RISING].drain_pos == DMA_CAPTURE_BUFLEN - 1);
    publish(SUB_FALLING, 150);
    timer.SR = 0;
    service_channel(&channel, false);
    assert(channel.count == 0 && output_count() == 1);
    expect_output(0, SUB_RISING, 42, 300);
  }
}

static void test_sparse_tail_and_strict_cutoff(void) {
  reset(3, 2, 0);
  publish(SUB_RISING, timer.CNT);
  service_channel(&channel, false);
  assert(channel.count == 2 && output_count() == 0);
  timer.CNT++;
  service_channel(&channel, false);
  assert(channel.count == 0 && output_count() == 1);
  expect_output(0, SUB_RISING, 42, timer.CNT - 1);
  service_channel(&channel, false);
  assert(output_count() == 1);

  // A long quiet interval does not require the opposite polarity before a lone tail is emitted.
  seconds_A = seconds_B = 45;
  publish(SUB_FALLING, 100);
  channel.count = 2;
  service_channel(&channel, false);
  assert(output_count() == 2);
  expect_output(1, SUB_FALLING, 45, 100);
}

static void change_source_during_snapshot(unsigned access) {
  if (access == 2) {
    dma[SUB_RISING].source++;
  }
}

static void change_falling_during_snapshot(unsigned access) {
  if (access == 4) {
    dma[SUB_FALLING].source++;
    dma[SUB_FALLING].destination++;
  }
}

static void test_snapshot_proof_and_wrap(void) {
  reset(3, 0, 0);
  capture_window_t window = capture_window(&channel);
  assert(window.cutoff_valid && window.end[0] == 0 && window.end[1] == 0);
  assert(window.cutoff.seconds == 42 && window.cutoff.counter == timer.CNT);

  for (unsigned s = 0; s < NUM_SUBS; s++) {
    reset(3, 0, DMA_CAPTURE_BUFLEN - 1);
    dma[s].source = 0;
    window = capture_window(&channel);
    assert(!window.cutoff_valid);  // FIFO data at a circular boundary is still outstanding.
    dma[s].destination = DMA_CAPTURE_BUFLEN;
    window = capture_window(&channel);
    assert(window.cutoff_valid && window.end[s] == 0);
    dma[s].destination = 0;
    window = capture_window(&channel);
    assert(window.cutoff_valid && window.end[s] == 0);
  }

  reset(3, 0, 0);
  access_hook = change_source_during_snapshot;
  window = capture_window(&channel);
  assert(!window.cutoff_valid);
  reset(3, 0, 0);
  access_hook = change_falling_during_snapshot;
  window = capture_window(&channel);
  assert(!window.cutoff_valid);
}

static void test_clear_discards_late_prefix(void) {
  reset(3, 2, 0);
  channel.slope = TIMESTAMPER_SLOPE_RISING;
  publish(SUB_RISING, 100);
  dma[SUB_RISING].source = 2;  // A second pre-clear record is still in flight.
  timer.CNT = 500;
  begin_capture_epoch(&channel);
  assert(channel.count == 0 && channel.sub[SUB_RISING].drain_pos == 1);
  publish(SUB_RISING, 400);
  publish(SUB_RISING, 600);
  publish(SUB_RISING, 800);
  service_channel(&channel, false);
  assert(channel.count == 2 && output_count() == 0);
  assert(!channel.sub[SUB_RISING].discard_before_epoch);
  publish(SUB_RISING, 900);
  service_channel(&channel, false);
  assert(output_count() == 1);
  expect_output(0, SUB_RISING, 42, 900);

  reset(3, 2, DMA_CAPTURE_BUFLEN - 1);
  seconds_A = 101;
  seconds_B = 100;
  timer.CNT = 100;
  dma[SUB_RISING].source = 0;
  begin_capture_epoch(&channel);
  publish(SUB_RISING, CLOCK_FREQ_HZ - 1);
  publish(SUB_RISING, 100);  // Exactly the new epoch, so this input must be counted.
  publish(SUB_FALLING, 110);
  timer.CNT = 200;
  service_channel(&channel, false);
  assert(channel.count == 2 && output_count() == 0);
  publish(SUB_RISING, 120);
  service_channel(&channel, false);
  assert(output_count() == 1);
  expect_output(0, SUB_RISING, 101, 120);
}

static void test_config_finishes_completed_old_epoch(void) {
  reset(3, 2, 0);
  publish(SUB_FALLING, 100);
  timer.SR = 1;  // Old rising capture is not in memory yet.
  service_channel(&channel, true);
  assert(output_count() == 1);
  expect_output(0, SUB_FALLING, 42, 100);
  channel.slope = TIMESTAMPER_SLOPE_RISING;
  channel.divider = 2;
  timer.CNT = 500;
  begin_capture_epoch(&channel);
  publish(SUB_RISING, 200);  // Old in-flight edge does not enter the new group.
  publish(SUB_RISING, 600);
  publish(SUB_RISING, 700);
  publish(SUB_FALLING, 650);  // Now-deselected sub-stream does not affect phase.
  timer.SR = 0;
  service_channel(&channel, false);
  assert(channel.count == 0 && output_count() == 2);
  expect_output(1, SUB_RISING, 42, 700);
  assert(channel.sub[SUB_FALLING].drain_pos == dma[SUB_FALLING].destination);
}

static void test_selected_ring_loss_and_continuation(void) {
  for (unsigned space = 0; space <= 1; space++) {
    reset(3, 2, DMA_CAPTURE_BUFLEN - 2);
    ts_tail = (ts_head + space + 1) & (TIMESTAMP_BUFLEN - 1);
    for (unsigned i = 0; i < 8; i++) {
      publish(i % 2, 100 + 10 * i);
    }
    service_channel(&channel, false);
    assert(channel.count == 1 && output_count() == space);
    assert(channel.buf_overflows == 3 - space);
    if (space) {
      expect_output(0, SUB_RISING, 42, 100);
    }
    ts_tail = (ts_tail + 1) & (TIMESTAMP_BUFLEN - 1);
    publish(SUB_RISING, 180);
    publish(SUB_FALLING, 190);
    service_channel(&channel, false);
    assert(channel.count == 0 && output_count() == space + 1);
    expect_output(space, SUB_FALLING, 42, 190);
  }
}

static void test_non_alternating_captured_edges(void) {
  reset(2, 0, 0);
  publish(SUB_RISING, 100);
  publish(SUB_RISING, 200);  // A missing physical fall must not break chronological selection.
  publish(SUB_FALLING, 250);
  publish(SUB_FALLING, 270);
  publish(SUB_RISING, 300);
  service_channel(&channel, false);
  assert(output_count() == 2 && channel.count == 1);
  expect_output(0, SUB_RISING, 42, 200);
  expect_output(1, SUB_FALLING, 42, 270);
}

static void test_fast_paths_do_not_sample_source_progress(void) {
  const timestamper_slope_t slopes[] = {TIMESTAMPER_SLOPE_RISING, TIMESTAMPER_SLOPE_FALLING,
                                        TIMESTAMPER_SLOPE_BOTH};
  for (unsigned i = 0; i < sizeof(slopes) / sizeof(slopes[0]); i++) {
    reset(1, 0, 0);
    channel.slope = slopes[i];
    publish(SUB_RISING, 100);
    publish(SUB_RISING, 300);
    publish(SUB_FALLING, 200);
    publish(SUB_FALLING, 400);
    service_channel(&channel, false);
    assert(accesses == 2);  // Just one completed-destination cursor read per sub-stream.
    if (slopes[i] == TIMESTAMPER_SLOPE_BOTH) {
      assert(output_count() == 4);
      expect_output(0, SUB_RISING, 42, 100);
      expect_output(1, SUB_RISING, 42, 300);
      expect_output(2, SUB_FALLING, 42, 200);
      expect_output(3, SUB_FALLING, 42, 400);
    } else {
      unsigned s = slopes[i] == TIMESTAMPER_SLOPE_RISING ? SUB_RISING : SUB_FALLING;
      assert(output_count() == 2);
      expect_output(0, s, 42, s == SUB_RISING ? 100 : 200);
      expect_output(1, s, 42, s == SUB_RISING ? 300 : 400);
    }
  }
}

static void test_seconds_update_boundaries(void) {
  const uint32_t boundaries[] = {CLOCK_FREQ_HZ / 4, CLOCK_FREQ_HZ * 3 / 4};
  for (unsigned i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++) {
    reset(2, 0, 0);
    if (i == 0) {
      seconds_B = 41;
    }
    publish(SUB_RISING, boundaries[i] - 100);
    timer.CNT = boundaries[i] - 50;
    timer.SR = 2;
    service_channel(&channel, false);
    assert(output_count() == 0 && channel.count == 0);
    if (i == 0) {
      seconds_B = seconds_A;
    } else {
      seconds_A++;
    }
    publish(SUB_FALLING, boundaries[i] + 100);
    timer.CNT = boundaries[i] + 200;
    timer.SR = 0;
    service_channel(&channel, false);
    assert(output_count() == 1 && channel.count == 0);
    expect_output(0, SUB_FALLING, 42, boundaries[i] + 100);
  }

  reset(2, 0, 0);
  seconds_A = 0;
  seconds_B = UINT32_MAX;
  publish(SUB_RISING, CLOCK_FREQ_HZ - 1);
  publish(SUB_FALLING, 0);
  service_channel(&channel, false);
  assert(output_count() == 1);
  expect_output(0, SUB_FALLING, 0, 0);
}

static uint32_t random_state = 7;
static uint32_t random_u32(void) {
  random_state = random_state * 1664525U + 1013904223U;
  return random_state;
}

static void test_many_split_batches_across_dma_wrap(void) {
  const uint32_t dividers[] = {2, 3, 5, UINT32_MAX};
  const uint32_t total = DMA_CAPTURE_BUFLEN * 3 + 7;
  const uint32_t start = CLOCK_FREQ_HZ - 1000;
  for (unsigned d = 0; d < sizeof(dividers) / sizeof(dividers[0]); d++) {
    uint32_t divider = dividers[d];
    reset(divider, divider - 1, DMA_CAPTURE_BUFLEN - 3);
    seconds_A = 43;
    seconds_B = 42;
    uint32_t published[NUM_SUBS] = {0}, consumed[NUM_SUBS] = {0};
    uint64_t next_selected = 0;
    unsigned iterations = 0;
    while (published[0] != total || published[1] != total) {
      assert(iterations++ < total * 4);
      unsigned s = (random_u32() >> 16) % 2;
      if (published[s] == total || published[s] - consumed[s] == DMA_CAPTURE_BUFLEN - 1) {
        s ^= 1;
      }
      uint32_t space = DMA_CAPTURE_BUFLEN - 1 - (published[s] - consumed[s]);
      uint32_t count = 1 + (random_u32() >> 16) % 3;
      if (count > space) {
        count = space;
      }
      if (count > total - published[s]) {
        count = total - published[s];
      }
      assert(count > 0);
      while (count--) {
        publish(s, (start + (published[s] * 2 + s) * 100) % CLOCK_FREQ_HZ);
        published[s]++;
      }
      bool complete = published[0] == total && published[1] == total;
      timer.SR = complete ? 0 : 3;
      timer.CNT = (start + total * 2 * 100 + 100) % CLOCK_FREQ_HZ;
      uint32_t before[NUM_SUBS] = {channel.sub[0].drain_pos, channel.sub[1].drain_pos};
      service_channel(&channel, false);
      for (s = 0; s < NUM_SUBS; s++) {
        consumed[s] += (channel.sub[s].drain_pos - before[s]) & (DMA_CAPTURE_BUFLEN - 1);
      }
      for (unsigned i = 0; i < output_count(); i++) {
        uint64_t tick = start + next_selected * 100;
        expect_output(i, next_selected % 2, 42 + tick / CLOCK_FREQ_HZ, tick % CLOCK_FREQ_HZ);
        next_selected += divider;
      }
      ts_tail = initial_head = ts_head;
      assert(channel.buf_overflows == 0);
    }
    assert(consumed[0] == total && consumed[1] == total);
    assert(channel.count == ((uint64_t)divider - 1 + total * 2) % divider);
    checked++;
  }
}

static void test_batch_order_rollover_and_dividers(void) {
  const uint32_t dividers[] = {2, 3, 5, 7, UINT32_MAX};
  const unsigned length = DMA_CAPTURE_BUFLEN - 1;
  for (unsigned d = 0; d < sizeof(dividers) / sizeof(dividers[0]); d++) {
    const uint32_t divider = dividers[d];
    const uint32_t carries[] = {0, 1, divider - 1};
    for (unsigned carry = 0; carry < sizeof(carries) / sizeof(carries[0]); carry++) {
      for (unsigned first = 0; first < NUM_SUBS; first++) {
        for (unsigned rollover = 0; rollover < 2; rollover++) {
          reset(divider, carries[carry], DMA_CAPTURE_BUFLEN - 3);
          uint32_t start = rollover ? CLOCK_FREQ_HZ - 1000 : 1000;
          if (rollover) {
            seconds_A = 43;
            seconds_B = 42;
          }
          for (unsigned group = 0; group < 2; group++) {
            unsigned s = group ^ first;
            for (unsigned i = 0; i < length; i++) {
              publish(s, (start + (i * 2 + s) * 100) % CLOCK_FREQ_HZ);
            }
            timer.SR = group == 0 ? 3 : 0;
            service_channel(&channel, false);
            if (group == 0) {
              assert(output_count() == 0 && channel.count == carries[carry]);
            }
          }
          uint32_t progress = carries[carry];
          unsigned expected = 0;
          for (unsigned i = 0; i < length * 2; i++) {
            if (++progress == divider) {
              progress = 0;
              uint32_t tick = start + i * 100;
              expect_output(expected++, i % 2, 42 + tick / CLOCK_FREQ_HZ, tick % CLOCK_FREQ_HZ);
            }
          }
          assert(output_count() == expected && channel.count == progress);
          assert(channel.buf_overflows == 0);
          checked++;
        }
      }
    }
  }
}

int main(void) {
  test_delayed_opposite_capture();
  test_sparse_tail_and_strict_cutoff();
  test_snapshot_proof_and_wrap();
  test_clear_discards_late_prefix();
  test_config_finishes_completed_old_epoch();
  test_selected_ring_loss_and_continuation();
  test_non_alternating_captured_edges();
  test_fast_paths_do_not_sample_source_progress();
  test_seconds_update_boundaries();
  test_many_split_batches_across_dma_wrap();
  test_batch_order_rollover_and_dividers();
  printf(
      "timestamper service: pending-DMA/epoch/tail cases and %u exact chronological traces "
      "passed (DMA %u, ring %u)\n",
      checked, DMA_CAPTURE_BUFLEN, TIMESTAMP_BUFLEN);
}
