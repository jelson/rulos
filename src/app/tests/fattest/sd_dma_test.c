#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "periph/sdcard2/fatfs_rulos.c"

struct rulos_dma_channel {
  rulos_dma_config_t config;
  bool allocated, running;
  uint8_t *memory;
  uint32_t count, remaining;
};

SPI_TypeDef sd_test_spi;
static struct rulos_dma_channel dma[2];
static uint8_t transmitted[512];
static unsigned allocations, starts, wakeups, busy_polls, fifo_bytes;
static bool overrun, complete_on_enable;
static enum { SUCCESS, RX_ERROR, TX_ERROR, DONE_THEN_ERROR, ERROR_THEN_DONE } scenario;

rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config) {
  unsigned idx = config->request == RULOS_DMA_REQ_SPI1_TX;
  assert(idx || config->request == RULOS_DMA_REQ_SPI1_RX);
  if (dma[idx].allocated) {
    return NULL;
  }
  dma[idx].allocated = true;
  dma[idx].config = *config;
  allocations++;
  return &dma[idx];
}

void rulos_dma_reconfigure(rulos_dma_channel_t *ch, const rulos_dma_config_t *config) {
  assert(ch->allocated && !ch->running);
  assert(config->request == ch->config.request);
  assert(config->periph_width == RULOS_DMA_WIDTH_BYTE);
  assert(config->mem_width == RULOS_DMA_WIDTH_BYTE);
  assert(config->mode == RULOS_DMA_MODE_NORMAL);
  assert(!config->periph_increment);
  ch->config = *config;
}

void rulos_dma_start(rulos_dma_channel_t *ch, volatile void *peripheral, void *memory,
                     uint32_t count) {
  assert(peripheral == &SPI1->DR);
  assert(ch->allocated && !ch->running && count > 0 && count <= sizeof(transmitted));
  ch->memory = memory;
  ch->count = ch->remaining = count;
  ch->running = true;
  starts++;
}

void rulos_dma_stop(rulos_dma_channel_t *ch) {
  assert(ch->running);
  if (scenario == SUCCESS) {
    assert(ch->remaining == 0);
  }
  ch->running = false;
}

static void callback(void (*fn)(void *), struct rulos_dma_channel *ch) {
  if (fn) {
    fn(ch->config.user_data);
  }
}

static void finish_rx(void) {
  for (unsigned i = 0; i < dma[0].count; i++) {
    dma[0].memory[dma[0].config.mem_increment ? i : 0] = (uint8_t)(i ^ 0xa5);
  }
  dma[0].remaining = 0;
  callback(dma[0].config.tc_callback, &dma[0]);
}

void sd_test_idle(void) {
  assert(SPI1->dma_rx && SPI1->dma_tx && dma[0].running && dma[1].running);
  assert(wakeups++ < 2);
  if (wakeups == 1) {
    for (unsigned i = 0; i < dma[1].count; i++) {
      transmitted[i] = dma[1].memory[dma[1].config.mem_increment ? i : 0];
    }
    dma[1].remaining = 0;
    callback(dma[1].config.tc_callback, &dma[1]);
    if (scenario == SUCCESS) {
      // TX is complete while RX still needs its final SPI clocks.
      return;
    }
    busy_polls = 2;
    struct rulos_dma_channel *failed = scenario == RX_ERROR ? &dma[0] : &dma[1];
    if (scenario == DONE_THEN_ERROR) {
      finish_rx();
    }
    callback(failed->config.error_callback, failed);
    if (scenario == ERROR_THEN_DONE) {
      finish_rx();
    }
  } else {
    assert(scenario == SUCCESS);
    finish_rx();
  }
}

void LL_SPI_EnableDMAReq_TX(SPI_TypeDef *spi) {
  assert(spi == SPI1 && spi->dma_rx);
  assert(dma[0].config.direction == RULOS_DMA_DIR_PERIPH_TO_MEM);
  assert(dma[1].config.direction == RULOS_DMA_DIR_MEM_TO_PERIPH);
  spi->dma_tx = true;
  if (complete_on_enable) {
    sd_test_idle();
    sd_test_idle();
  }
}

bool LL_SPI_IsActiveFlag_BSY(SPI_TypeDef *spi) {
  assert(spi == SPI1 && !spi->dma_rx && !spi->dma_tx);
  if (busy_polls) {
    if (--busy_polls == 0) {
      fifo_bytes = 2;
      overrun = true;
    }
    return true;
  }
  return false;
}

bool LL_SPI_IsActiveFlag_RXNE(SPI_TypeDef *spi) {
  assert(spi == SPI1 && busy_polls == 0);
  return fifo_bytes != 0;
}

uint8_t LL_SPI_ReceiveData8(SPI_TypeDef *spi) {
  assert(spi == SPI1 && fifo_bytes > 0);
  fifo_bytes--;
  return 0;
}

void LL_SPI_ClearFlag_OVR(SPI_TypeDef *spi) {
  assert(spi == SPI1 && fifo_bytes == 0 && busy_polls == 0);
  overrun = false;
}

static void test_transfer(bool read, unsigned count) {
  uint8_t buffer[sizeof(transmitted)];
  memset(buffer, 0x5a, sizeof(buffer));
  wakeups = 0;
  starts = 0;
  bool ok =
      read ? TM_SPI_ReadMulti(SPI1, buffer, 0xff, count) : TM_SPI_WriteMulti(SPI1, buffer, count);
  assert(ok == (scenario == SUCCESS));
  assert(!dma[0].running && !dma[1].running && !SPI1->dma_rx && !SPI1->dma_tx);
  assert(busy_polls == 0 && fifo_bytes == 0 && !overrun);
  assert(starts == 2 && wakeups == (scenario == SUCCESS ? 2 : 1));
  if (ok) {
    for (unsigned i = 0; i < count; i++) {
      assert(transmitted[i] == (read ? 0xff : 0x5a));
      assert(buffer[i] == (read ? (uint8_t)(i ^ 0xa5) : 0x5a));
    }
  }
}

int main(void) {
  TM_SPI_Init();
  assert(allocations == 2);
  const unsigned sizes[] = {16, 512};
  for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    for (unsigned read = 0; read < 2; read++) {
      for (scenario = SUCCESS; scenario <= ERROR_THEN_DONE; scenario++) {
        test_transfer(read, sizes[i]);
      }
      scenario = SUCCESS;
      complete_on_enable = true;
      test_transfer(read, sizes[i]);
      complete_on_enable = false;
    }
  }
  starts = wakeups = 0;
  assert(TM_SPI_ReadMulti(SPI1, NULL, 0xff, 0));
  assert(TM_SPI_WriteMulti(SPI1, NULL, 0));
  assert(starts == 0 && wakeups == 0 && allocations == 2);
  puts("sd DMA: RX completion, error ordering, FIFO cleanup, and read/write recovery passed");
}
