/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/logging.h"

static void dispatch_int(uint8_t uart_id);
static void dispatch_dma(uint8_t uart_id);

//////////////////////////////////////////////////////////////////////////////

/// Configuration

typedef struct {
  USART_TypeDef *instance;
  uint32_t instance_irqn;
  GPIO_TypeDef *rx_port;
  uint32_t rx_pin;
  GPIO_TypeDef *tx_port;
  uint32_t tx_pin;
  DMA_Channel_TypeDef *tx_dma_chan;
  uint32_t tx_dma_request;
  uint32_t tx_dma_irqn;
  uint32_t altfunc;
} stm32_uart_config_t;

///////////// stm32f0

#if defined(RULOS_ARM_stm32f0)

#include "stm32f0xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_10,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_9,
        .tx_dma_chan = DMA1_Channel2,
        .tx_dma_irqn = DMA1_Channel2_3_IRQn,
        .altfunc = GPIO_AF1_USART1,
    },
};

void USART1_IRQHandler() {
  dispatch_int(0);
}

void DMA1_Channel2_3_IRQHandler() {
  dispatch_dma(0);
}

///////////// stm32f1

#elif defined(RULOS_ARM_stm32f1)

#include "stm32f1xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_10,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_9,
        .tx_dma_chan = DMA1_Channel4,
        .tx_dma_irqn = DMA1_Channel4_IRQn,
    },
};

void USART1_IRQHandler() {
  dispatch_int(0);
}

void DMA1_Channel4_IRQHandler() {
  dispatch_dma(0);
}

///////////// stm32f3

#elif defined(RULOS_ARM_stm32f3)

void USART1_IRQHandler() {
  dispatch_int(0);
}

void DMA1_Channel4_IRQHandler() {
  dispatch_dma(0);
}

#include "stm32f3xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_10,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_9,
        .tx_dma_chan = DMA1_Channel4,
        .tx_dma_irqn = DMA1_Channel4_IRQn,
        .altfunc = GPIO_AF7_USART1,
    },
};

///////////// stm32g0

#elif defined(RULOS_ARM_stm32g0)

#include "stm32g0xx.h"
#include "stm32g0xx_hal_dma.h"
#include "stm32g0xx_ll_usart.h"

void USART1_IRQHandler() {
  dispatch_int(0);
}

void DMA1_Channel1_IRQHandler() {
  dispatch_dma(0);
}

int xxint = 0;

#if STM32G0B1xx
#define USART2_IRQn USART2_LPUART2_IRQn
void USART2_LPUART2_IRQHandler() {
  xxint++;
  dispatch_int(1);
}

int xxdma = 0;

void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler() {
  xxdma++;
  dispatch_dma(1);
  dispatch_dma(2);
  dispatch_dma(3);
  dispatch_dma(4);
  dispatch_dma(5);
}

void USART3_4_5_6_LPUART1_IRQHandler() {
  dispatch_int(2);
  dispatch_int(3);
  dispatch_int(4);
  dispatch_int(5);
}

#else
void USART2_IRQHandler() {
  dispatch_int(1);
}
void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler() {
  dispatch_dma(1);
}
#endif

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_10,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_9,
        .tx_dma_chan = DMA1_Channel1,
        .tx_dma_irqn = DMA1_Channel1_IRQn,
        .tx_dma_request = DMA_REQUEST_USART1_TX,
        .altfunc = GPIO_AF1_USART1,
    },
    {
        .instance = USART2,
        .instance_irqn = USART2_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_3,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_2,
        .tx_dma_chan = DMA1_Channel4,
#if STM32G0B1xx
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
#else
        .tx_dma_irqn = DMA1_Ch4_5_DMAMUX1_OVR_IRQn,
#endif
        .tx_dma_request = DMA_REQUEST_USART2_TX,
        .altfunc = GPIO_AF1_USART2,
    },
#ifdef USART3
    {
        .instance = USART3,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_port = GPIOB,
        .rx_pin = GPIO_PIN_9,
        .tx_port = GPIOB,
        .tx_pin = GPIO_PIN_2,
        .tx_dma_chan = DMA1_Channel5,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART3_TX,
        .altfunc = GPIO_AF4_USART3,
    },
#endif
#ifdef USART4
    {
        .instance = USART4,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_1,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_0,
        .tx_dma_chan = DMA1_Channel6,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART4_TX,
        .altfunc = GPIO_AF4_USART4,
    },
#endif
#ifdef USART5
    {
        .instance = USART5,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_port = GPIOB,
        .rx_pin = GPIO_PIN_1,
        .tx_port = GPIOB,
        .tx_pin = GPIO_PIN_0,
        .tx_dma_chan = DMA1_Channel7,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART5_TX,
        .altfunc = GPIO_AF8_USART5,
    },
#endif
#ifdef USART6
    {
        .instance = USART6,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_5,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_4,
        .tx_dma_chan = DMA1_Channel1,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART6_TX,
        .altfunc = GPIO_AF3_USART6,
    },
#endif
};

///////////// stm32g4

#elif defined(RULOS_ARM_stm32g4)

#include "stm32g4xx.h"
#include "stm32g4xx_hal_dma.h"
#include "stm32g4xx_ll_usart.h"

void USART1_IRQHandler() {
  dispatch_int(0);
}

void DMA1_Channel1_IRQHandler() {
  dispatch_dma(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_port = GPIOA,
        .rx_pin = GPIO_PIN_10,
        .tx_port = GPIOA,
        .tx_pin = GPIO_PIN_9,
        .tx_dma_chan = DMA1_Channel1,
        .tx_dma_irqn = DMA1_Channel1_IRQn,
        .tx_dma_request = DMA_REQUEST_USART1_TX,
        .altfunc = GPIO_AF7_USART1,
    },
};

/////////////////////////////////////////

#else
#error "Tell the UART code about your chip's UART."
#include <stophere>
#endif

typedef struct {
  // housekeeping
  bool initted;
  bool rx_gpio_initted;
  bool tx_gpio_initted;
  uint32_t frame_errors;
  uint32_t parity_errors;
  uint32_t noise_errors;

  // HAL API: the upcall to get more data and the user data for that callback
  uint8_t uart_id;
  hal_uart_receive_cb rx_cb;
  hal_uart_next_sendbuf_cb next_sendbuf_cb;
  void *user_data;

  // UART handle from STM32's HAL
  UART_HandleTypeDef hal_uart_handle;

  // Transmit and receive DMA buffers and DMA handles
  DMA_HandleTypeDef hal_dma_tx_handle;
#if USE_DMA_FOR_RX
  DMA_HandleTypeDef hal_dma_rx_handle;
#endif
} stm32_uart_t;

// Eventually this should be conditional depending on what kind of
// chip you have
#define NUM_UARTS (sizeof(stm32_uart_config) / sizeof(stm32_uart_config[0]))
static stm32_uart_t g_stm32_uarts[NUM_UARTS] = {};

///// interrupt handlers

static void dispatch_dma(uint8_t uart_id) {
  const stm32_uart_t *u = &g_stm32_uarts[uart_id];
  if (!u->initted) {
    return;
  }

  HAL_DMA_IRQHandler(u->hal_uart_handle.hdmatx);
}

static void dispatch_int(uint8_t uart_id) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *c = &stm32_uart_config[uart_id];
  if (!u->initted) {
    return;
  }

  // clear RX errors not handled through HAL
  if (LL_USART_IsActiveFlag_FE(c->instance)) {
    LL_USART_ClearFlag_FE(c->instance);
    u->frame_errors++;
  }
  if (LL_USART_IsActiveFlag_PE(c->instance)) {
    LL_USART_ClearFlag_PE(c->instance);
    u->parity_errors++;
  }
  if (LL_USART_IsActiveFlag_NE(c->instance)) {
    LL_USART_ClearFlag_NE(c->instance);
    u->noise_errors++;
  }

  // dispatch rx upcall, if needed -- not handled through the HAL
  if (LL_USART_IsActiveFlag_RXNE(c->instance) &&
      LL_USART_IsEnabledIT_RXNE(c->instance)) {
    // note: we have to read the character whether or not we send it anywhere;
    // reading the char is what clears the interrupt
    char inchar = LL_USART_ReceiveData8(c->instance);
    if (u->rx_cb != NULL) {
      u->rx_cb(uart_id, u->user_data, inchar);
    }
  }

  // dispatch the rest of the interrupt handling through the HAL
  HAL_UART_IRQHandler(&u->hal_uart_handle);
}

/////// transmission

static void maybe_launch_next_tx(stm32_uart_t *uart) {
  assert(uart->next_sendbuf_cb != NULL);
  assert(HAL_UART_GetState(&uart->hal_uart_handle) == HAL_UART_STATE_READY);

  // Ask the layer above for the next buffer to send by calling the "send ready"
  // callback
  const char *buf;
  uint16_t len;
  uart->next_sendbuf_cb(uart->uart_id, uart->user_data, &buf, &len);

  if (len == 0) {
    // tx train complete!
    uart->next_sendbuf_cb = NULL;
  } else {
    // Start the transfer
    if (HAL_UART_Transmit_DMA(&uart->hal_uart_handle, (void *)buf, len) !=
        HAL_OK) {
      __builtin_trap();
    }
  }
}

// Callback called when TX is complete. This overrides a weak symbol in the HAL
// implementation.
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *hal_uart_handle) {
  for (int i = 0; i < NUM_UARTS; i++) {
    if (&g_stm32_uarts[i].hal_uart_handle == hal_uart_handle) {
      maybe_launch_next_tx(&g_stm32_uarts[i]);
    }
  }
}

static void config_gpio(const stm32_uart_config_t *config, bool rx);

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  if (!u->tx_gpio_initted) {
    const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
    config_gpio(config, false);
    u->tx_gpio_initted = true;
  }
  assert(u->next_sendbuf_cb == NULL);
  u->next_sendbuf_cb = cb;
  maybe_launch_next_tx(u);
}

///// reception

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb) {
  /*
   * Use the Low-Level libraries (not the HAL) to set up the RX interrupt. Yeah,
   * it's a little weird to be using a mix of both HAL and LL. I wanted to use
   * the HAL libraries for the TX DMA, since it's complex. For RX, the HAL
   * libraries don't really do what we want - you have to give them a total
   * length you want to read, and then it shuts everything down again. We want
   * to just start receiving and never stop, with a simple interrupt every time
   * we receive a character.
   */
  assert(uart_id < NUM_UARTS);
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
  if (!u->rx_gpio_initted) {
    config_gpio(config, true);
    u->rx_gpio_initted = true;
  }
  u->rx_cb = rx_cb;
  LL_USART_EnableIT_RXNE(config->instance);
}

///// initialization

// We configure the GPIO only on-demand, i.e. configure the rx pin on the first
// rx and the tx pin on the first tx. This makes it easy to do half-duplex,
// e.g. use the tx pin for something else if you only plan to receive on the
// uart.
static void config_gpio(const stm32_uart_config_t *config, bool rx) {
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#if !defined(RULOS_ARM_stm32f1)
  GPIO_InitStruct.Alternate = config->altfunc;
#endif

  if (rx) {
    GPIO_InitStruct.Pin = config->rx_pin;
    HAL_GPIO_Init(config->rx_port, &GPIO_InitStruct);
  } else {
    GPIO_InitStruct.Pin = config->tx_pin;
    HAL_GPIO_Init(config->tx_port, &GPIO_InitStruct);
  }
}

void hal_uart_init(uint8_t uart_id, uint32_t baud,
                   void *user_data /* for both rx and tx upcalls */,
                   uint16_t *max_tx_len /* OUT */) {
  assert(uart_id < NUM_UARTS);
  stm32_uart_t *uart = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
  uart->initted = true;
  uart->uart_id = uart_id;
  uart->user_data = user_data;
  *max_tx_len = 65535;

  // Note: RULOS HAL uarts are numbered starting from 0.
  // STM32 UARTs are numbered starting from 1.

  // Enable USART clock
  switch (uart_id) {
#ifdef USART1
    case 0:
      __HAL_RCC_USART1_CLK_ENABLE();
      break;
#endif

#ifdef USART2
    case 1:
      __HAL_RCC_USART2_CLK_ENABLE();
      break;
#endif

#ifdef USART3
    case 2:
      __HAL_RCC_USART3_CLK_ENABLE();
      break;
#endif

#ifdef USART4
    case 3:
      __HAL_RCC_USART4_CLK_ENABLE();
      break;
#endif

#ifdef USART5
    case 4:
      __HAL_RCC_USART5_CLK_ENABLE();
      break;
#endif

#ifdef USART6
    case 5:
      __HAL_RCC_USART6_CLK_ENABLE();
      break;
#endif
    default:
      assert(false);
      break;
  }

  // Configure UART HAL
  uart->hal_uart_handle.Instance = config->instance;
  uart->hal_uart_handle.Init.BaudRate = baud;
  uart->hal_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
  uart->hal_uart_handle.Init.Parity = UART_PARITY_NONE;
  uart->hal_uart_handle.Init.StopBits = UART_STOPBITS_1;
  uart->hal_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart->hal_uart_handle.Init.Mode = UART_MODE_TX_RX;
#if defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32g0)
  uart->hal_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
  uart->hal_uart_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  uart->hal_uart_handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  uart->hal_uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
#endif
  if (HAL_UART_Init(&uart->hal_uart_handle) != HAL_OK) {
    __builtin_trap();
  }

  // Configure the DMA controller for TX
  uart->hal_dma_tx_handle.Instance = config->tx_dma_chan;
#if defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32g0)
  uart->hal_dma_tx_handle.Init.Request = config->tx_dma_request;
#endif
  uart->hal_dma_tx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
  uart->hal_dma_tx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
  uart->hal_dma_tx_handle.Init.MemInc = DMA_MINC_ENABLE;
  uart->hal_dma_tx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  uart->hal_dma_tx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  uart->hal_dma_tx_handle.Init.Mode = DMA_NORMAL;
  uart->hal_dma_tx_handle.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&uart->hal_dma_tx_handle) != HAL_OK) {
    __builtin_trap();
  }

  // Associate the initialized DMA handle to the UART handle
  __HAL_LINKDMA(&uart->hal_uart_handle, hdmatx, uart->hal_dma_tx_handle);

  // Set up DMA interrupt
  HAL_NVIC_SetPriority(config->tx_dma_irqn, 0, 1);
  HAL_NVIC_EnableIRQ(config->tx_dma_irqn);

  // Set up USART peripheral interrupt
  HAL_NVIC_SetPriority(config->instance_irqn, 0, 1);
  HAL_NVIC_EnableIRQ(config->instance_irqn);
}
