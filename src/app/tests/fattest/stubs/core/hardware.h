#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  volatile uint32_t DR;
  volatile uint32_t SR;
  bool dma_rx, dma_tx;
} SPI_TypeDef;
extern SPI_TypeDef sd_test_spi;
#define SPI1       (&sd_test_spi)
#define SPI_SR_BSY 1U
#define GPIO_B6    6
#define __INLINE   inline

void gpio_set(int pin);
void gpio_clr(int pin);
void LL_SPI_TransmitData8(SPI_TypeDef *spi, uint8_t byte);
uint8_t LL_SPI_ReceiveData8(SPI_TypeDef *spi);

enum {
  GPIOB,
  LL_GPIO_PIN_3,
  LL_GPIO_PIN_4,
  LL_GPIO_PIN_5,
  LL_GPIO_AF_5,
  LL_GPIO_MODE_ALTERNATE,
  LL_GPIO_SPEED_FREQ_HIGH,
  LL_GPIO_PULL_DOWN,
  LL_GPIO_PULL_UP,
  LL_APB2_GRP1_PERIPH_SPI1,
  LL_SPI_BAUDRATEPRESCALER_DIV256,
  LL_SPI_BAUDRATEPRESCALER_DIV4,
  LL_SPI_FULL_DUPLEX,
  LL_SPI_PHASE_1EDGE,
  LL_SPI_POLARITY_LOW,
  LL_SPI_MSB_FIRST,
  LL_SPI_DATAWIDTH_8BIT,
  LL_SPI_NSS_SOFT,
  LL_SPI_RX_FIFO_TH_QUARTER,
  LL_SPI_MODE_MASTER,
};

static inline void sd_test_gpio_config(int port, int pin, int value) {
  (void)port;
  (void)pin;
  (void)value;
}
#define LL_GPIO_SetPinMode   sd_test_gpio_config
#define LL_GPIO_SetAFPin_0_7 sd_test_gpio_config
#define LL_GPIO_SetPinSpeed  sd_test_gpio_config
#define LL_GPIO_SetPinPull   sd_test_gpio_config

static inline void sd_test_spi_config(SPI_TypeDef *spi, int value) {
  (void)spi;
  (void)value;
}
#define LL_SPI_SetBaudRatePrescaler sd_test_spi_config
#define LL_SPI_SetTransferDirection sd_test_spi_config
#define LL_SPI_SetClockPhase        sd_test_spi_config
#define LL_SPI_SetClockPolarity     sd_test_spi_config
#define LL_SPI_SetTransferBitOrder  sd_test_spi_config
#define LL_SPI_SetDataWidth         sd_test_spi_config
#define LL_SPI_SetNSSMode           sd_test_spi_config
#define LL_SPI_SetRxFIFOThreshold   sd_test_spi_config
#define LL_SPI_SetMode              sd_test_spi_config

static inline void LL_SPI_Disable(SPI_TypeDef *spi) {
  (void)spi;
}
static inline void LL_SPI_Enable(SPI_TypeDef *spi) {
  (void)spi;
}
static inline void LL_APB2_GRP1_EnableClock(int clock) {
  (void)clock;
}
static inline void gpio_make_output(int pin) {
  (void)pin;
}
static inline uintptr_t LL_SPI_DMA_GetRegAddr(SPI_TypeDef *spi) {
  return (uintptr_t)&spi->DR;
}
static inline void LL_SPI_EnableDMAReq_RX(SPI_TypeDef *spi) {
  spi->dma_rx = true;
}
static inline void LL_SPI_DisableDMAReq_RX(SPI_TypeDef *spi) {
  spi->dma_rx = false;
}
static inline void LL_SPI_DisableDMAReq_TX(SPI_TypeDef *spi) {
  spi->dma_tx = false;
}
void LL_SPI_EnableDMAReq_TX(SPI_TypeDef *spi);
bool LL_SPI_IsActiveFlag_BSY(SPI_TypeDef *spi);
bool LL_SPI_IsActiveFlag_RXNE(SPI_TypeDef *spi);
void LL_SPI_ClearFlag_OVR(SPI_TypeDef *spi);
