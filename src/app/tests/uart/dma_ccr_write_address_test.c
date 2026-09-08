// Exercise the production classic-DMA accessor for every memory width and circular endpoint.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static unsigned barriers;
#define __DMB() ((void)barriers++)

enum {
  LL_DMA_MDATAALIGN_BYTE = 0,
  LL_DMA_MDATAALIGN_HALFWORD = 0x400,
  LL_DMA_MDATAALIGN_WORD = 0x800,
};

typedef struct {
  uint32_t base[2];
  uint32_t remaining[2];
  uint32_t width[2];
} DMA_TypeDef;
typedef struct {
  DMA_TypeDef *dma;
  uint32_t ll_channel;
} dma_channel_hw_t;
typedef struct {
  uint32_t nitems;
} dma_channel_state_t;
typedef dma_channel_state_t rulos_dma_channel_t;

static DMA_TypeDef regs;
static const dma_channel_hw_t g_hw[] = {{&regs, 0}, {&regs, 1}};
static dma_channel_state_t g_state[2];

static int state_to_idx(const rulos_dma_channel_t *ch) {
  return ch - g_state;
}

static uint32_t LL_DMA_GetDataLength(DMA_TypeDef *dma, uint32_t channel) {
  assert(dma == &regs && channel < 2);
  return dma->remaining[channel];
}

static uint32_t LL_DMA_GetMemorySize(DMA_TypeDef *dma, uint32_t channel) {
  assert(dma == &regs && channel < 2);
  return dma->width[channel];
}

static uint32_t LL_DMA_GetMemoryAddress(DMA_TypeDef *dma, uint32_t channel) {
  assert(dma == &regs && channel < 2);
  return dma->base[channel];
}

#include "chip/arm/stm32/core/dma_write_address_impl.h"

int main(void) {
  const uint32_t widths[] = {LL_DMA_MDATAALIGN_BYTE, LL_DMA_MDATAALIGN_HALFWORD,
                             LL_DMA_MDATAALIGN_WORD};
  const uint32_t lengths[] = {1, 16, 2048, 65535};
  for (unsigned ch = 0; ch < 2; ch++) {
    regs.base[ch] = 0x20001000 + ch * 0x1000;
    for (unsigned n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
      g_state[ch].nitems = lengths[n];
      for (unsigned w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
        regs.width[ch] = widths[w];
        for (uint32_t committed = 0; committed <= lengths[n]; committed++) {
          regs.remaining[ch] = lengths[n] - committed;
          assert(rulos_dma_get_write_address(&g_state[ch]) ==
                 regs.base[ch] + committed * (1U << w));
        }
        regs.remaining[ch] = lengths[n];
        assert(rulos_dma_get_write_address(&g_state[ch]) == regs.base[ch]);
      }
    }
  }
  assert(barriers > 0);
  puts("dma: classic completed addresses account for memory width and circular reloads");
}
