// Exercise the production H5 completed-write accessor independently of source/FIFO progress.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define RULOS_ARM_stm32h5
static unsigned barriers;
#define __DMB() ((void)barriers++)

typedef struct {
  uint32_t destination[2];
  uint32_t remaining[2];
} DMA_TypeDef;
typedef struct {
  DMA_TypeDef *dma;
  uint32_t ll_channel;
} dma_channel_hw_t;
typedef struct {
  unsigned index;
} rulos_dma_channel_t;

static DMA_TypeDef regs;
static const dma_channel_hw_t g_hw[] = {{&regs, 0}, {&regs, 1}};
static const rulos_dma_channel_t channels[] = {{0}, {1}};

static int state_to_idx(const rulos_dma_channel_t *ch) {
  return ch->index;
}

static uint32_t LL_DMA_GetDestAddress(DMA_TypeDef *dma, uint32_t channel) {
  assert(dma == &regs && channel < 2);
  return dma->destination[channel];
}

#include "chip/arm/stm32/core/dma_write_address_impl.h"

int main(void) {
  const uint32_t base = 0x20001000;
  const uint32_t lengths[] = {16, 2048};
  const uint32_t widths[] = {1, 2, 4};
  for (unsigned ch = 0; ch < 2; ch++) {
    for (unsigned n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
      for (unsigned w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
        uint32_t length = lengths[n], width = widths[w];
        for (uint32_t committed = 0; committed <= length; committed++) {
          regs.destination[ch] = base + committed * width;
          // Source progress may lead destination progress by several FIFO words, including BNDT
          // reaching zero while the final destination write has not completed.
          regs.remaining[ch] = committed + 8 < length ? length - committed - 8 : 0;
          assert(rulos_dma_get_write_address(&channels[ch]) == base + committed * width);
        }
        regs.destination[ch] = base;
        regs.remaining[ch] = length * width;
        assert(rulos_dma_get_write_address(&channels[ch]) == base);
      }
    }
  }
  assert(barriers > 0);
  puts("dma: H5 completed addresses exclude source/FIFO progress and preserve circular endpoints");
}
