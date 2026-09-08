// Private capture service implementation. Include after channel/ring state and the fast drains.
#pragma once

static bool sub_active(timestamper_slope_t slope, uint8_t s) {
  return slope == TIMESTAMPER_SLOPE_BOTH ||
         (slope == TIMESTAMPER_SLOPE_FALLING ? s == SUB_FALLING : s == SUB_RISING);
}

CCMRAM static inline timestamp_t capture_timestamp(uint32_t counter) {
  return (timestamp_t){counter < CLOCK_FREQ_HZ / 2 ? seconds_A : seconds_B, counter};
}

CCMRAM static inline bool capture_before(timestamp_t a, timestamp_t b) {
  return a.seconds == b.seconds ? a.counter < b.counter : (int32_t)(a.seconds - b.seconds) < 0;
}

// CDAR publishes completed destination writes, unlike H5 BNDT's source-side progress. The same-
// priority DMA ISRs and irq-guarded task service serialize ownership of the software drain cursor.
CCMRAM static inline uint32_t safe_cur(channel_t *chan, uint8_t s) {
  uintptr_t address = rulos_dma_get_write_address(chan->sub[s].dma_ch);
  return ((address - (uintptr_t)chan->sub[s].buf) / sizeof(uint32_t)) & (DMA_CAPTURE_BUFLEN - 1);
}

CCMRAM static inline uint32_t source_cur(channel_t *chan, uint8_t s) {
  return (DMA_CAPTURE_BUFLEN - rulos_dma_get_remaining(chan->sub[s].dma_ch)) &
         (DMA_CAPTURE_BUFLEN - 1);
}

typedef struct {
  uint32_t end[NUM_SUBS];
  timestamp_t cutoff;
  bool cutoff_valid;
} capture_window_t;

CCMRAM static capture_window_t capture_window(channel_t *chan) {
  uint32_t before[NUM_SUBS];
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    before[s] = source_cur(chan, s);
  }
  capture_window_t window;
  window.cutoff = capture_timestamp(chan->capture_tim->CNT);
  window.cutoff_valid = !(chan->capture_tim->SR & chan->capture_if_mask);
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    window.end[s] = safe_cur(chan, s);
    uint32_t after = source_cur(chan, s);
    window.cutoff_valid &= before[s] == after && after == window.end[s];
  }
  // A clear CCIF excludes unfetched captures; unchanged source positions matching completed
  // destinations exclude older data still in flight. Later arrivals are beyond the sampled CNT.
  // A busy snapshot simply leaves an uncertain tail for the next IRQ/100 ms periodic service.
  return window;
}

// Clears/reconfiguration skip completed prefixes now and late old writes as they arrive. Once
// the first new-epoch record is reached, no per-record epoch checks remain in the hot path.
static void begin_capture_epoch(channel_t *chan) {
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    chan->sub[s].drain_pos = safe_cur(chan, s);
    chan->sub[s].discard_before_epoch = true;
  }
  chan->capture_epoch = capture_timestamp(chan->capture_tim->CNT);
  chan->count = 0;
}

CCMRAM static void discard_old_prefix(channel_t *chan, uint8_t s, uint32_t end) {
  if (!chan->sub[s].discard_before_epoch) {
    return;
  }
  uint32_t pos = chan->sub[s].drain_pos;
  while (pos != end) {
    if (!capture_before(capture_timestamp(chan->sub[s].buf[pos]), chan->capture_epoch)) {
      chan->sub[s].discard_before_epoch = false;
      break;
    }
    pos = (pos + 1) & (DMA_CAPTURE_BUFLEN - 1);
  }
  chan->sub[s].drain_pos = pos;
}

CCMRAM static void drain_both_divided(channel_t *chan, const capture_window_t *window,
                                      bool finish_epoch) {
  uint32_t pos[NUM_SUBS];
  timestamp_t next[NUM_SUBS];
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    pos[s] = chan->sub[s].drain_pos;
    if (pos[s] != window->end[s]) {
      next[s] = capture_timestamp(chan->sub[s].buf[pos[s]]);
    }
  }
  uint32_t head = ts_head;
  uint32_t available = (ts_tail - head - 1) % TIMESTAMP_BUFLEN;
  while (pos[SUB_RISING] != window->end[SUB_RISING] ||
         pos[SUB_FALLING] != window->end[SUB_FALLING]) {
    uint8_t s;
    if (pos[SUB_RISING] != window->end[SUB_RISING] &&
        pos[SUB_FALLING] != window->end[SUB_FALLING]) {
      s = capture_before(next[SUB_FALLING], next[SUB_RISING]) ? SUB_FALLING : SUB_RISING;
    } else {
      s = pos[SUB_RISING] != window->end[SUB_RISING] ? SUB_RISING : SUB_FALLING;
      if (!finish_epoch && !(window->cutoff_valid && capture_before(next[s], window->cutoff))) {
        break;
      }
    }
    chan->recent_pulse = true;
    if (++chan->count == chan->divider) {
      chan->count = 0;
      if (available) {
        timestamp_buffer[head].seconds = next[s].seconds;
        timestamp_buffer[head].counter = next[s].counter | chan->sub[s].counter_tag;
        head = (head + 1) % TIMESTAMP_BUFLEN;
        available--;
      } else {
        chan->buf_overflows++;
      }
    }
    pos[s] = (pos[s] + 1) & (DMA_CAPTURE_BUFLEN - 1);
    if (pos[s] != window->end[s]) {
      next[s] = capture_timestamp(chan->sub[s].buf[pos[s]]);
    }
  }
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    chan->sub[s].drain_pos = pos[s];
  }
  ts_head = head;
}

// No sub-stream may lap its software cursor, and service latency must remain within the A/B
// seconds scheme's 250 ms margin. Capture-envelope inputs are serviced at every half-buffer and
// every 100 ms. Only divided BOTH needs a merge; all other modes retain their specialized drains.
// finish_epoch is used only with the pair disarmed: consume completed old captures, then let the
// new epoch discard any still-in-flight old records instead of waiting with interrupts masked.
CCMRAM static void service_channel(channel_t *chan, bool finish_epoch) {
  if (chan->slope == TIMESTAMPER_SLOPE_BOTH && chan->divider != 1) {
    capture_window_t window = capture_window(chan);
    for (uint8_t s = 0; s < NUM_SUBS; s++) {
      discard_old_prefix(chan, s, window.end[s]);
    }
    drain_both_divided(chan, &window, finish_epoch);
    return;
  }
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    uint32_t end = safe_cur(chan, s);
    if (sub_active(chan->slope, s)) {
      discard_old_prefix(chan, s, end);
      drain_sub_fast(chan, s, end);
    } else {
      chan->sub[s].drain_pos = end;
    }
  }
}
