#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DO_NOT_USE_DMA
#include "periph/sdcard2/fatfs_rulos.c"

SPI_TypeDef sd_test_spi;
static uint8_t transmitted[16];
static unsigned sent;

void LL_SPI_TransmitData8(SPI_TypeDef *spi, uint8_t byte) {
  assert(spi == SPI1 && sent < sizeof(transmitted));
  transmitted[sent] = byte;
  spi->DR = sent++;
}

uint8_t LL_SPI_ReceiveData8(SPI_TypeDef *spi) {
  assert(spi == SPI1);
  return spi->DR;
}

int main(void) {
  uint8_t buffer[16];
  assert(TM_SPI_ReadMulti(SPI1, buffer, 0xff, sizeof(buffer)));
  assert(sent == sizeof(buffer));
  for (unsigned i = 0; i < sizeof(buffer); i++) {
    assert(buffer[i] == i && transmitted[i] == 0xff);
  }
  sent = 0;
  assert(TM_SPI_WriteMulti(SPI1, buffer, sizeof(buffer)));
  assert(sent == sizeof(buffer) && memcmp(buffer, transmitted, sizeof(buffer)) == 0);
  sent = 0;
  assert(TM_SPI_ReadMulti(SPI1, NULL, 0xff, 0));
  assert(TM_SPI_WriteMulti(SPI1, NULL, 0));
  assert(sent == 0);
  puts("sd polling: byte reads, writes, and empty transfers passed");
}
