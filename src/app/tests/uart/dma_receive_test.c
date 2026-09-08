// Run the production DMA IRQ dispatcher and UART RX implementation with fake registers/state.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "periph/uart/uart_hal.h"

#define CCMRAM
#define USE_RX_DMA_FOR_CHIP 1
enum { TC = 1, HT = 2, TE = 4, RX_SIZE = 64, HALF_SIZE = RX_SIZE / 2 };

typedef struct {
  unsigned flags;
  uint32_t remaining;
} DMA_TypeDef;
typedef struct {
  DMA_TypeDef *dma;
  uint32_t ll_channel;
} dma_channel_hw_t;
typedef struct {
  bool allocated;
  bool circular;
  uint32_t nitems;
  void (*tc_callback)(void *);
  void (*ht_callback)(void *);
  void (*error_callback)(void *);
  void *user_data;
} dma_channel_state_t;
typedef dma_channel_state_t rulos_dma_channel_t;

static DMA_TypeDef regs;
static const dma_channel_hw_t g_hw[] = {{&regs, 0}};
static dma_channel_state_t g_state[1];

#define FLAG_HELPERS(name, flag)                                            \
  static bool ll_dma_is_active_flag_##name(DMA_TypeDef *dma, uint32_t ch) { \
    assert(ch == 0);                                                        \
    return dma->flags & flag;                                               \
  }                                                                         \
  static void ll_dma_clear_flag_##name(DMA_TypeDef *dma, uint32_t ch) {     \
    assert(ch == 0);                                                        \
    dma->flags &= ~flag;                                                    \
  }
FLAG_HELPERS(tc, TC)
FLAG_HELPERS(ht, HT)
FLAG_HELPERS(te, TE)

static bool LL_DMA_IsEnabledIT_HT(DMA_TypeDef *dma, uint32_t ch) {
  assert(dma == &regs && ch == 0);
  return true;
}
static uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  assert(ch == &g_state[0]);
  return regs.remaining;
}

#include "chip/arm/stm32/core/dma_irq_impl.h"

typedef struct {
  bool initted;
  hal_uart_receive_cb rx_cb;
  void *user_data;
  char *rx_buf;
  size_t rx_half_buflen;
  bool rx_cb_ready;
  bool rx_data_ready;
  uint32_t rx_dma_processed_pos;
  uint32_t tot_ints;
  uint32_t tot_rx_bytes;
  uint32_t dropped_rx_bytes;
  uint16_t max_chars_per_rx_batch;
} stm32_uart_t;

static stm32_uart_t g_stm32_uarts[1];

#include "chip/arm/stm32/periph/uart/uart_rx_impl.h"

static char rx_buf[RX_SIZE];
static struct {
  char snapshot[HALF_SIZE];
  size_t len;
  size_t offset;
} deliveries[8];
static unsigned delivery_count;
static bool immediate_done;

static void on_receive(uint8_t uart_id, void *user_data, char *buf, size_t len) {
  assert(uart_id == 0 && user_data == &regs);
  assert(len > 0 && len <= HALF_SIZE);
  assert(buf >= rx_buf && buf + len <= rx_buf + RX_SIZE);
  assert(!g_stm32_uarts[0].rx_cb_ready);
  assert(delivery_count < sizeof(deliveries) / sizeof(deliveries[0]));
  deliveries[delivery_count].offset = buf - rx_buf;
  deliveries[delivery_count].len = len;
  memcpy(deliveries[delivery_count++].snapshot, buf, len);
  if (immediate_done) {
    g_stm32_uarts[0].rx_cb_ready = true;
  }
}

static void setup(void) {
  regs = (DMA_TypeDef){.remaining = RX_SIZE};
  g_state[0] = (dma_channel_state_t){.allocated = true,
                                     .circular = true,
                                     .nitems = RX_SIZE,
                                     .tc_callback = hal_uart_on_rx_dma_tc,
                                     .ht_callback = hal_uart_on_rx_dma_ht,
                                     .user_data = (void *)(uintptr_t)0};
  g_stm32_uarts[0] = (stm32_uart_t){.initted = true,
                                    .rx_cb = on_receive,
                                    .user_data = &regs,
                                    .rx_buf = rx_buf,
                                    .rx_half_buflen = HALF_SIZE,
                                    .rx_cb_ready = true};
  for (unsigned i = 0; i < RX_SIZE; i++) {
    rx_buf[i] = (char)i;
  }
  memset(deliveries, 0, sizeof(deliveries));
  delivery_count = 0;
  immediate_done = false;
}

static void interrupt(unsigned flags, uint32_t remaining) {
  regs.flags = flags;
  regs.remaining = remaining;
  dispatch_channel_irq(0);
  assert(regs.flags == 0);
}

static void idle(uint32_t remaining) {
  stm32_uart_t *u = &g_stm32_uarts[0];
  assert(u->rx_cb_ready);
  u->rx_data_ready = true;
  rx_dma_flush(0, u, remaining);
  assert(!u->rx_data_ready);
}

static void done(void) {
  assert(!g_stm32_uarts[0].rx_cb_ready);
  g_stm32_uarts[0].rx_cb_ready = true;
}

static void test_half_first(void) {
  const uint32_t positions[] = {0, RX_SIZE, RX_SIZE - 8};
  for (unsigned i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
    setup();
    immediate_done = true;
    interrupt(HT | TC, positions[i]);
    assert(delivery_count == 2);
    assert(deliveries[0].offset == 0 && deliveries[0].len == HALF_SIZE);
    assert(deliveries[1].offset == HALF_SIZE && deliveries[1].len == HALF_SIZE);
    assert(g_stm32_uarts[0].rx_dma_processed_pos == 0);
    assert(g_stm32_uarts[0].tot_ints == 2);
    assert(g_stm32_uarts[0].tot_rx_bytes == RX_SIZE);
    assert(g_stm32_uarts[0].dropped_rx_bytes == 0);
  }
}

static void test_tc_first(void) {
  const uint32_t positions[] = {HALF_SIZE, HALF_SIZE - 8};
  for (unsigned i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
    for (unsigned prefix = 0; prefix <= 8; prefix += 8) {
      setup();
      if (prefix) {
        idle(RX_SIZE - prefix);
        assert(deliveries[0].len == prefix);
        done();
      }
      unsigned before = delivery_count;
      memset(rx_buf, 'n', HALF_SIZE);
      interrupt(TC | HT, positions[i]);
      assert(delivery_count == before + 1);
      assert(deliveries[before].offset == 0 && deliveries[before].len == HALF_SIZE);
      assert(memcmp(deliveries[before].snapshot, rx_buf, HALF_SIZE) == 0);
      stm32_uart_t *u = &g_stm32_uarts[0];
      assert(u->rx_dma_processed_pos == HALF_SIZE);
      assert(u->tot_rx_bytes == RX_SIZE + HALF_SIZE);
      assert(u->dropped_rx_bytes == RX_SIZE - prefix);
      assert(u->max_chars_per_rx_batch == HALF_SIZE);
      done();
      interrupt(TC, RX_SIZE);
      assert(delivery_count == before + 2);
      assert(deliveries[before + 1].offset == HALF_SIZE);
      assert(u->rx_dma_processed_pos == 0);
      assert(u->tot_rx_bytes == 2 * RX_SIZE);
      assert(u->dropped_rx_bytes == RX_SIZE - prefix);
    }
  }
}

static void test_pending_callback(void) {
  setup();
  interrupt(HT | TC, RX_SIZE);
  assert(delivery_count == 1);
  assert(g_stm32_uarts[0].dropped_rx_bytes == HALF_SIZE);
  // The first callback still owns the deferred snapshot while DMA and later IRQs continue.
  char snapshot[HALF_SIZE];
  memcpy(snapshot, deliveries[0].snapshot, sizeof(snapshot));
  memset(rx_buf, 'x', sizeof(rx_buf));
  interrupt(TC | HT, HALF_SIZE);
  assert(delivery_count == 1);
  assert(memcmp(snapshot, deliveries[0].snapshot, sizeof(snapshot)) == 0);
  assert(g_stm32_uarts[0].tot_rx_bytes == 2 * RX_SIZE + HALF_SIZE);
  assert(g_stm32_uarts[0].dropped_rx_bytes == 2 * RX_SIZE);
  assert(g_stm32_uarts[0].rx_dma_processed_pos == HALF_SIZE);
  done();
  interrupt(TC, RX_SIZE);
  assert(delivery_count == 2 && deliveries[1].len == HALF_SIZE);
  assert(deliveries[1].offset == HALF_SIZE);
  assert(g_stm32_uarts[0].rx_dma_processed_pos == 0);
}

static void test_idle_boundaries(void) {
  setup();
  immediate_done = true;
  idle(RX_SIZE - 8);
  idle(RX_SIZE - 20);
  idle(RX_SIZE - 40);  // Clamp at HT; IDLE cannot steal the next half.
  assert(delivery_count == 3);
  assert(deliveries[0].offset == 0 && deliveries[0].len == 8);
  assert(deliveries[1].offset == 8 && deliveries[1].len == 12);
  assert(deliveries[2].offset == 20 && deliveries[2].len == 12);
  assert(g_stm32_uarts[0].rx_dma_processed_pos == HALF_SIZE);
  interrupt(HT, HALF_SIZE);  // The already-flushed half must not be delivered twice.
  assert(delivery_count == 3);
  idle(RX_SIZE - 40);
  assert(deliveries[3].offset == HALF_SIZE && deliveries[3].len == 8);
  idle(0);
  assert(delivery_count == 4 && g_stm32_uarts[0].rx_dma_processed_pos == 40);
  interrupt(TC, RX_SIZE);
  assert(deliveries[4].offset == 40 && deliveries[4].len == 24);
  assert(g_stm32_uarts[0].tot_rx_bytes == RX_SIZE);
  assert(g_stm32_uarts[0].dropped_rx_bytes == 0);
  assert(g_stm32_uarts[0].max_chars_per_rx_batch == 24);
}

int main(void) {
  test_half_first();
  test_tc_first();
  test_pending_callback();
  test_idle_boundaries();
  setup();
  g_stm32_uarts[0].initted = false;
  interrupt(HT | TC, HALF_SIZE);
  assert(delivery_count == 0 && g_stm32_uarts[0].tot_ints == 0);
  puts("uart DMA: bounded half-buffer snapshots, delayed boundaries, IDLE and recovery");
}
