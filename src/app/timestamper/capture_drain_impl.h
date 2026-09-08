// Private capture drain implementation. Include after channel_t and the timestamp ring state.
#pragma once

// Divide the complete DMA batch before charging ring space. A dropped output still consumes its
// input group, so later batches retain the same phase even while the ring is full.
CCMRAM static void drain_sub_divided(channel_t *chan, uint8_t s, uint32_t pos, uint32_t count) {
  const uint32_t divider = chan->divider;
  const uint32_t first = divider - chan->count;
  if (count < first) {
    // count + progress is strictly below divider, including when divider is UINT32_MAX.
    chan->count += count;
    return;
  }
  count -= first;
  uint32_t selected = 1 + count / divider;
  chan->count = count % divider;

  uint32_t head = ts_head;
  uint32_t available = (ts_tail - head - 1) % TIMESTAMP_BUFLEN;
  if (selected > available) {
    chan->buf_overflows += selected - available;
    selected = available;
  }

  const uint32_t tag = chan->sub[s].counter_tag;
  volatile uint32_t *const buf = chan->sub[s].buf;
  pos = (pos + first - 1) & (DMA_CAPTURE_BUFLEN - 1);
  while (selected > 0) {
    uint32_t counter = buf[pos];
    uint32_t seconds = counter < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;
    timestamp_buffer[head].seconds = seconds;
    timestamp_buffer[head].counter = counter | tag;
    head = (head + 1) % TIMESTAMP_BUFLEN;
    pos = (pos + divider) & (DMA_CAPTURE_BUFLEN - 1);
    selected--;
  }
  ts_head = head;
}

// Drain sub s from drain_pos up to cur. Keep the undivided hot loop at its original load, A/B
// seconds select, two stores and masked increment; divided inputs use a separate strided loop.
CCMRAM static void drain_sub_fast(channel_t *chan, uint8_t s, uint32_t cur) {
  uint32_t pos = chan->sub[s].drain_pos;
  if (pos == cur) {
    return;
  }
  chan->recent_pulse = true;
  chan->sub[s].drain_pos = cur;

  uint32_t count = (cur - pos) & (DMA_CAPTURE_BUFLEN - 1);
  if (__builtin_expect(chan->divider != 1, 0)) {
    drain_sub_divided(chan, s, pos, count);
    return;
  }

  uint32_t head = ts_head;
  uint32_t available = (ts_tail - head - 1) % TIMESTAMP_BUFLEN;
  if (__builtin_expect(count > available, 0)) {
    chan->buf_overflows += count - available;
    count = available;
    if (count == 0) {
      return;
    }
  }

  const uint32_t tag = chan->sub[s].counter_tag;
  volatile uint32_t *const buf = chan->sub[s].buf;

  while (count > 0) {
    uint32_t seg = DMA_CAPTURE_BUFLEN - pos;
    if (seg > count) {
      seg = count;
    }
    // Pointer-based iteration so the compiler doesn't spill the count.
    volatile uint32_t *src = buf + pos;
    volatile uint32_t *const end = src + seg;
    while (src < end) {
      uint32_t counter = *src++;
      uint32_t seconds = counter < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;
      timestamp_buffer[head].seconds = seconds;
      timestamp_buffer[head].counter = counter | tag;
      head = (head + 1) % TIMESTAMP_BUFLEN;
    }
    pos = (pos + seg) & (DMA_CAPTURE_BUFLEN - 1);
    count -= seg;
  }
  ts_head = head;
}
