#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  volatile uint32_t DR;
  volatile uint32_t SR;
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
