// Exercise the real shared IRQ dispatcher with fake channel registers.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CCMRAM
enum { TC = 1, HT = 2, TE = 4 };
typedef struct {
  unsigned flags;
  bool ht_enabled;
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
static const dma_channel_hw_t g_hw[] = {{&regs, 0}, {NULL, 0}};
static dma_channel_state_t g_state[2];

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
  assert(ch == 0);
  return dma->ht_enabled;
}
static uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  assert(ch == &g_state[0]);
  return regs.remaining;
}

#include "chip/arm/stm32/core/dma_irq_impl.h"

static char callbacks[8];
static unsigned callback_count;
static bool stop_on_callback;
static unsigned arrive_on_callback;

static void record(char event, unsigned flag, void *data) {
  assert(data == &regs);
  assert(!(regs.flags & flag));
  assert(callback_count < sizeof(callbacks) - 1);
  callbacks[callback_count++] = event;
  if (stop_on_callback) {
    regs.flags = 0;
  }
  regs.flags |= arrive_on_callback;
  arrive_on_callback = 0;
}
static void on_tc(void *data) {
  record('T', TC, data);
}
static void on_ht(void *data) {
  record('H', HT, data);
}
static void on_error(void *data) {
  record('E', TE, data);
}
static void setup(unsigned flags, unsigned remaining) {
  regs = (DMA_TypeDef){.flags = flags, .ht_enabled = true, .remaining = remaining};
  g_state[0] = (dma_channel_state_t){.allocated = true,
                                     .circular = true,
                                     .nitems = 64,
                                     .tc_callback = on_tc,
                                     .ht_callback = on_ht,
                                     .error_callback = on_error,
                                     .user_data = &regs};
  memset(callbacks, 0, sizeof(callbacks));
  callback_count = 0;
  stop_on_callback = false;
  arrive_on_callback = 0;
}
static void check(unsigned flags, unsigned remaining, const char *expected) {
  setup(flags, remaining);
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, expected) == 0);
  assert(regs.flags == 0);
}

int main(void) {
  check(TC | HT, 64, "HT");
  check(TC | HT, 40, "HT");
  check(TC | HT, 0, "HT");
  check(TC | HT, 32, "TH");
  check(TC | HT, 16, "TH");
  check(TC | HT | TE, 64, "HTE");
  check(HT, 32, "H");
  check(TC, 64, "T");
  check(TE, 64, "E");
  check(0, 64, "");

  setup(TC | HT, 0);
  g_state[0].circular = false;
  regs.ht_enabled = false;
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, "T") == 0);
  assert(regs.flags == HT);

  const unsigned positions[] = {64, 32};
  for (unsigned i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
    setup(TC | HT | TE, positions[i]);
    stop_on_callback = true;
    dispatch_channel_irq(0);
    assert(callback_count == 1);
    assert(regs.flags == 0);
  }

  setup(HT, 32);
  arrive_on_callback = TC;
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, "H") == 0 && regs.flags == TC);
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, "HT") == 0 && regs.flags == 0);

  setup(TC, 64);
  arrive_on_callback = HT;
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, "T") == 0 && regs.flags == HT);
  dispatch_channel_irq(0);
  assert(strcmp(callbacks, "TH") == 0 && regs.flags == 0);

  setup(TC | HT | TE, 32);
  g_state[0].tc_callback = g_state[0].ht_callback = g_state[0].error_callback = NULL;
  dispatch_channel_irq(0);
  assert(callback_count == 0 && regs.flags == 0);

  setup(TC | HT, 32);
  g_state[0].allocated = false;
  dispatch_channel_irq(0);
  dispatch_channel_irq(1);
  assert(callback_count == 0 && regs.flags == (TC | HT));
  puts("dma: pending boundaries follow DMA position and respect callback cancellation");
}
