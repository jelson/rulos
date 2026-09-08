#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CCMRAM
#define CLOCK_FREQ_HZ 250000000U

typedef struct {
  struct {
    volatile uint32_t *buf;
    uint32_t drain_pos;
    uint32_t counter_tag;
  } sub[2];
  uint32_t buf_overflows;
  uint32_t divider;
  uint32_t count;
  volatile bool recent_pulse;
} channel_t;

typedef struct {
  uint32_t seconds;
  uint32_t counter;
} timestamp_t;

static timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head, ts_tail;
static volatile uint32_t seconds_A = 101, seconds_B = 100;

#include "capture_drain_impl.h"

static uint32_t captures[2][DMA_CAPTURE_BUFLEN];
static channel_t channel, expected_channel;
static timestamp_t expected_buffer[TIMESTAMP_BUFLEN];
static uint32_t expected_head, expected_tail;
static uint32_t counter;
static unsigned batches_checked;

static void reset(uint32_t divider, uint32_t progress, uint32_t available, uint32_t pos) {
  assert(divider > 0 && progress < divider);
  assert(available < TIMESTAMP_BUFLEN && pos < DMA_CAPTURE_BUFLEN);
  memset(timestamp_buffer, 0xa5, sizeof(timestamp_buffer));
  memcpy(expected_buffer, timestamp_buffer, sizeof(expected_buffer));
  memset(&channel, 0, sizeof(channel));
  for (unsigned s = 0; s < 2; s++) {
    channel.sub[s].buf = captures[s];
    channel.sub[s].drain_pos = pos;
    channel.sub[s].counter_tag = (2U << 30) | (s == 0 ? 1U << 29 : 0);
  }
  channel.divider = divider;
  channel.count = progress;
  expected_channel = channel;
  ts_head = expected_head = TIMESTAMP_BUFLEN - 3;
  ts_tail = expected_tail = (ts_head + available + 1) % TIMESTAMP_BUFLEN;
  counter = CLOCK_FREQ_HZ / 2 - 12;
}

// Deliberately model one input at a time, independently of the production batch/stride arithmetic.
static void reference_input(unsigned s, uint32_t captured) {
  if (expected_channel.divider != 1) {
    expected_channel.count++;
    if (expected_channel.count != expected_channel.divider) {
      return;
    }
    expected_channel.count = 0;
  }
  uint32_t next = (expected_head + 1) % TIMESTAMP_BUFLEN;
  if (next == expected_tail) {
    expected_channel.buf_overflows++;
    return;
  }
  timestamp_t *record = &expected_buffer[expected_head];
  record->seconds = captured < CLOCK_FREQ_HZ / 2 ? seconds_A : seconds_B;
  record->counter = captured | expected_channel.sub[s].counter_tag;
  expected_head = next;
}

static void append_batch(unsigned s, uint32_t count) {
  assert(count < DMA_CAPTURE_BUFLEN);
  uint32_t pos = channel.sub[s].drain_pos;
  for (uint32_t i = 0; i < count; i++) {
    captures[s][pos] = counter;
    reference_input(s, counter);
    counter = (counter + 4) % CLOCK_FREQ_HZ;
    pos = (pos + 1) % DMA_CAPTURE_BUFLEN;
  }
  expected_channel.sub[s].drain_pos = pos;
  expected_channel.recent_pulse |= count > 0;

  drain_sub_fast(&channel, s, pos);

  assert(ts_head == expected_head);
  assert(ts_tail == expected_tail);
  assert(channel.count == expected_channel.count);
  assert(channel.buf_overflows == expected_channel.buf_overflows);
  assert(channel.recent_pulse == expected_channel.recent_pulse);
  for (unsigned sub = 0; sub < 2; sub++) {
    assert(channel.sub[sub].drain_pos == expected_channel.sub[sub].drain_pos);
  }
  assert(memcmp(timestamp_buffer, expected_buffer, sizeof(timestamp_buffer)) == 0);
  batches_checked++;
}

static void consume(uint32_t count) {
  assert(count <= (ts_head - ts_tail) % TIMESTAMP_BUFLEN);
  ts_tail = expected_tail = (ts_tail + count) % TIMESTAMP_BUFLEN;
}

static void test_raw_batch_exceeds_space_but_selected_outputs_fit(void) {
  uint32_t inputs = DMA_CAPTURE_BUFLEN - 1;
  uint32_t outputs = inputs / 3;
  reset(3, 0, outputs, DMA_CAPTURE_BUFLEN - 3);
  append_batch(0, inputs);
  assert(channel.buf_overflows == 0);
  assert(channel.count == inputs % 3);
}

static void test_full_ring_preserves_divider_progress(void) {
  reset(3, 1, 0, DMA_CAPTURE_BUFLEN - 3);
  append_batch(0, 7);
  assert(channel.buf_overflows == 2);
  assert(channel.count == 2);
  consume(1);
  append_batch(0, 1);
  assert(channel.count == 0);
  assert(channel.buf_overflows == 2);

  reset(5, 1, 0, 0);
  append_batch(0, 2);
  assert(channel.count == 3);
  assert(channel.buf_overflows == 0);
}

static void test_partial_capacity_counts_dropped_outputs(void) {
  reset(3, 2, 1, DMA_CAPTURE_BUFLEN - 2);
  append_batch(0, 8);
  assert(channel.buf_overflows == 2);
  assert(channel.count == 1);
  consume(1);
  append_batch(0, 2);
  assert(channel.buf_overflows == 2);
  assert(channel.count == 0);
}

static void test_uint32_max_divider(void) {
  reset(UINT32_MAX, UINT32_MAX - 2, 1, DMA_CAPTURE_BUFLEN - 1);
  append_batch(0, 3);
  assert(channel.count == 1);
  assert(channel.buf_overflows == 0);

  reset(UINT32_MAX, UINT32_MAX - 2, 0, DMA_CAPTURE_BUFLEN - 1);
  append_batch(0, 1);
  assert(channel.count == UINT32_MAX - 1);
  append_batch(0, 2);
  assert(channel.count == 1);
  assert(channel.buf_overflows == 1);

  reset(UINT32_MAX, 0, 0, 0);
  append_batch(0, DMA_CAPTURE_BUFLEN - 1);
  assert(channel.count == DMA_CAPTURE_BUFLEN - 1);
  assert(channel.buf_overflows == 0);
}

static void test_timestamp_rollover_and_tags(void) {
  for (unsigned s = 0; s < 2; s++) {
    reset(3, 2, 4, DMA_CAPTURE_BUFLEN - 2);
    counter = CLOCK_FREQ_HZ - 8;
    append_batch(s, 10);
    assert(timestamp_buffer[TIMESTAMP_BUFLEN - 3].seconds == seconds_B);
    assert(timestamp_buffer[TIMESTAMP_BUFLEN - 2].seconds == seconds_A);
    assert(timestamp_buffer[0].seconds == seconds_A);
  }
}

static void test_batch_split_and_dma_wrap(void) {
  const uint32_t dividers[] = {1, 2, 3, 5, 8, 31, UINT32_MAX};
  const uint32_t spaces[] = {0, 1, TIMESTAMP_BUFLEN - 1};
  const uint32_t total = DMA_CAPTURE_BUFLEN - 1;
  // Exercise empty, middle and boundary-adjacent splits in both buffer geometries.
  const uint32_t splits[] = {0, 1, 2, 3, total / 2, total - 3, total - 2, total - 1, total};
  for (unsigned d = 0; d < sizeof(dividers) / sizeof(dividers[0]); d++) {
    for (unsigned a = 0; a < sizeof(spaces) / sizeof(spaces[0]); a++) {
      for (unsigned carry = 0; carry < 2; carry++) {
        for (unsigned wrap = 0; wrap < 2; wrap++) {
          for (unsigned split = 0; split < sizeof(splits) / sizeof(splits[0]); split++) {
            reset(dividers[d], carry ? dividers[d] - 1 : 0, spaces[a],
                  wrap ? DMA_CAPTURE_BUFLEN - 3 : 0);
            append_batch(0, splits[split]);
            append_batch(0, total - splits[split]);
          }
        }
      }
    }
  }
}

static uint32_t random_state = 1;
static uint32_t random_u32(void) {
  random_state = random_state * 1664525U + 1013904223U;
  return random_state;
}

static void test_mixed_batches_and_ring_drains(void) {
  for (unsigned test = 0; test < 200; test++) {
    uint32_t divider = test % 3 ? 1 + random_u32() % 31 : UINT32_MAX;
    reset(divider, random_u32() % divider, random_u32() % TIMESTAMP_BUFLEN,
          random_u32() % DMA_CAPTURE_BUFLEN);
    for (unsigned batch = 0; batch < 20; batch++) {
      uint32_t occupied = (ts_head - ts_tail) % TIMESTAMP_BUFLEN;
      consume(random_u32() % (occupied + 1));
      append_batch(batch % 2, random_u32() % DMA_CAPTURE_BUFLEN);
    }
  }
}

int main(void) {
  reset(3, 2, 0, 0);
  append_batch(0, 0);
  assert(!channel.recent_pulse);
  test_raw_batch_exceeds_space_but_selected_outputs_fit();
  test_full_ring_preserves_divider_progress();
  test_partial_capacity_counts_dropped_outputs();
  test_uint32_max_divider();
  test_timestamp_rollover_and_tags();
  test_batch_split_and_dma_wrap();
  test_mixed_batches_and_ring_drains();
  printf("timestamper capture: %u production DMA batches passed (DMA %u, ring %u)\n",
         batches_checked, DMA_CAPTURE_BUFLEN, TIMESTAMP_BUFLEN);
  return 0;
}
