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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/logging.h"
#include "periph/uart/uart_hal.h"
#include "stm32.h"

#if defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32g4)
#define USE_RX_DMA_FOR_CHIP 1
#include "core/dma.h"
#else
#define USE_RX_DMA_FOR_CHIP 0
#endif

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  // housekeeping
  bool initted;
  bool rx_gpio_initted;
  bool tx_gpio_initted;

  // RULOS HAL API: the upcall to get more data and the user data for that
  // callback
  uint8_t uart_id;
  void *user_data;

  // tx
  hal_uart_next_sendbuf_cb next_sendbuf_cb;

  // rx
  hal_uart_receive_cb rx_cb;
  char *rx_buf;
  // buflen we're given by the layer above, divided by two: size of each half
  size_t rx_half_buflen;
  bool rx_cb_ready;    // has the previous call to rx_cb completed?
  bool rx_data_ready;  // have we received an idle interrupt?

  // rx--interrupt mode only
  char *rx_curr_base_buf;  // points to rx_buf, or halfway into rx_buf
  size_t rx_bytes_stored;  // number of bytes received

  // UART handle from STM32's HAL
  UART_HandleTypeDef hal_uart_handle;

  // Transmit and receive DMA buffers and DMA handles
  DMA_HandleTypeDef hal_dma_tx_handle;

  // statistics
  uint32_t tot_ints;
  uint32_t tot_rx_bytes;
  uint32_t tot_tx_bytes;
  uint32_t dropped_rx_bytes;
  uint32_t frame_errors;
  uint32_t parity_errors;
  uint32_t noise_errors;
  uint32_t overruns;
  uint32_t idle_ints;
  uint16_t min_chars_per_rx_isr;
  uint16_t max_chars_per_rx_isr;
  uint16_t max_chars_per_tx_batch;
  uint16_t max_chars_per_rx_batch;
} stm32_uart_t;

/// Configuration

typedef struct {
  // general
  USART_TypeDef *instance;
  IRQn_Type instance_irqn;

  // rx
  gpio_pin_t rx_pin;
  uint32_t rx_altfunc;
  DMA_TypeDef *rx_dma_instance;
  int rx_dma_channel;
  IRQn_Type rx_dma_irqn;
  uint32_t rx_dma_request;

  // tx
  gpio_pin_t tx_pin;
  uint32_t tx_altfunc;
  DMA_Channel_TypeDef *tx_dma_chan;
  IRQn_Type tx_dma_irqn;
  uint32_t tx_dma_request;
} stm32_uart_config_t;

static void on_usart_interrupt(uint8_t uart_id);
static void dispatch_tx_dma(uint8_t uart_id);
static void config_gpio(const stm32_uart_config_t *config, bool rx);

#if USE_RX_DMA_FOR_CHIP
static void on_rx_dma_interrupt(uint8_t uart_id);
#endif

///// pin selections

// rulos uart 0
#ifndef RULOS_UART0_RX_PIN
#define RULOS_UART0_RX_PIN GPIO_A10
#endif

#ifndef RULOS_UART0_TX_PIN
#define RULOS_UART0_TX_PIN GPIO_A9
#endif

// rulos uart 1
#ifndef RULOS_UART1_RX_PIN
#define RULOS_UART1_RX_PIN GPIO_A3
#endif

#ifndef RULOS_UART1_TX_PIN
#define RULOS_UART1_TX_PIN GPIO_A2
#endif

// rulos uart 2
#ifndef RULOS_UART2_RX_PIN
#define RULOS_UART2_RX_PIN GPIO_B9
#endif

#ifndef RULOS_UART2_TX_PIN
#define RULOS_UART2_TX_PIN GPIO_B2
#endif

// rulos uart 3
#ifndef RULOS_UART3_RX_PIN
#define RULOS_UART3_RX_PIN GPIO_A1
#endif

#ifndef RULOS_UART3_TX_PIN
#define RULOS_UART3_TX_PIN GPIO_A0
#endif

// rulos uart 4
#ifndef RULOS_UART4_RX_PIN
#define RULOS_UART4_RX_PIN GPIO_B1
#endif

#ifndef RULOS_UART4_TX_PIN
#define RULOS_UART4_TX_PIN GPIO_B0
#endif

// rulos uart 5
#ifndef RULOS_UART5_RX_PIN
#define RULOS_UART5_RX_PIN GPIO_A5
#endif

#ifndef RULOS_UART5_TX_PIN
#define RULOS_UART5_TX_PIN GPIO_A4
#endif



///////////// stm32f0

#if defined(RULOS_ARM_stm32f0)

#include "stm32f0xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF1_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF1_USART1,
        .tx_dma_chan = DMA1_Channel2,
        .tx_dma_irqn = DMA1_Channel2_3_IRQn,
    },
};

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

void DMA1_Channel2_3_IRQHandler() {
  dispatch_tx_dma(0);
}

///////////// stm32f1

#elif defined(RULOS_ARM_stm32f1)

#include "stm32f1xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_pin = RULOS_UART0_RX_PIN,
        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_dma_chan = DMA1_Channel4,
        .tx_dma_irqn = DMA1_Channel4_IRQn,
    },
};

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

void DMA1_Channel4_IRQHandler() {
  dispatch_tx_dma(0);
}

///////////// stm32f3

#elif defined(RULOS_ARM_stm32f3)

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

void DMA1_Channel4_IRQHandler() {
  dispatch_tx_dma(0);
}

#include "stm32f3xx_ll_usart.h"
static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF7_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF7_USART1,
        .tx_dma_chan = DMA1_Channel4,
        .tx_dma_irqn = DMA1_Channel4_IRQn,
    },
};

///////////// stm32g0

#elif defined(RULOS_ARM_stm32g0)

// Summary of stm32g0 uart DMA channels
//
// unit:channel

// (rulos_id) stm32_id  dma_rx  dma_tx
//      (0)      1        1:1     1:5
//      (1)      2        1:4     1:2(*)
//      (2)      3        2:1     1:3(*)
//      (3)      4        2:2     1:6
//      (4)      5        2:3     1:7
//      (5)      6        2:4     2:5
//
// (*) might get surrendered if sd card in use,
//     until dynamic dma allocation is implemented

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

void DMA1_Channel1_IRQHandler() {
  on_rx_dma_interrupt(0);  // DMA 1:1
}

#ifndef UART_SURRENDER_DMA1_CHAN2_3

void DMA1_Channel2_3_IRQHandler() {
  dispatch_tx_dma(1);  // DMA 1:2
#if STM32G0B1xx
  dispatch_tx_dma(2);  // DMA 1:3
#endif
}

#endif  // UART_SURRENDER_DMA1_CHAN2_3

#if STM32G0B1xx
#define USART2_IRQn USART2_LPUART2_IRQn
void USART2_LPUART2_IRQHandler() {
  on_usart_interrupt(1);
}

#define DMA1_Channel4_IRQn DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn
#define DMA1_Channel5_IRQn DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn

void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler() {
  on_rx_dma_interrupt(1);  // DMA 1:4
  dispatch_tx_dma(0);      // DMA 1:5
  dispatch_tx_dma(3);      // DMA 1:6
  dispatch_tx_dma(4);      // DMA 1:7

  on_rx_dma_interrupt(2);  // DMA 2:1
  on_rx_dma_interrupt(3);  // DMA 2:2
  on_rx_dma_interrupt(4);  // DMA 2:3
  on_rx_dma_interrupt(5);  // DMA 2:4
  dispatch_tx_dma(5);      // DMA 2:5
}

void USART3_4_5_6_LPUART1_IRQHandler() {
  on_usart_interrupt(2);
  on_usart_interrupt(3);
  on_usart_interrupt(4);
  on_usart_interrupt(5);
}

#else
void USART2_IRQHandler() {
  on_usart_interrupt(1);
}

#define DMA1_Channel4_IRQn DMA1_Ch4_5_DMAMUX1_OVR_IRQn
#define DMA1_Channel5_IRQn DMA1_Ch4_5_DMAMUX1_OVR_IRQn

void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler() {
  on_rx_dma_interrupt(1);  // DMA 1:4
  dispatch_tx_dma(0);      // DMA 1:5
}
#endif


static const stm32_uart_config_t stm32_uart_config[] = {
    {
      // rulos uart 0
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
#define GPIO_A10 123
#if RULOS_UART0_RX_PIN == GPIO_A10
        .rx_altfunc = GPIO_AF1_USART1,
#else
        .rx_altfunc = GPIO_AF0_USART1,
#endif
#undef GPIO_A10
        .rx_dma_instance = DMA1, // comment out to test interrupts
        .rx_dma_channel = LL_DMA_CHANNEL_1,
        .rx_dma_irqn = DMA1_Channel1_IRQn,
        .rx_dma_request = DMA_REQUEST_USART1_RX,

        .tx_pin = RULOS_UART0_TX_PIN,
#define GPIO_A9 123
#if RULOS_UART0_TX_PIN == GPIO_A9
        .tx_altfunc = GPIO_AF1_USART1,
#else
        .tx_altfunc = GPIO_AF0_USART1,
#endif
#undef GPIO_A9
        .tx_dma_chan = DMA1_Channel5,
        .tx_dma_irqn = DMA1_Channel5_IRQn,
        .tx_dma_request = DMA_REQUEST_USART1_TX,
    },
    {
      // rulos uart 1
        .instance = USART2,
        .instance_irqn = USART2_IRQn,
        .rx_pin = RULOS_UART1_RX_PIN,
        .rx_altfunc = GPIO_AF1_USART2,
        .rx_dma_instance = DMA1,
        .rx_dma_channel = LL_DMA_CHANNEL_4,
        .rx_dma_irqn = DMA1_Channel4_IRQn,
        .rx_dma_request = DMA_REQUEST_USART2_RX,

#ifndef temp_disabled_UART_SURRENDER_DMA1_CHAN2_3
        .tx_pin = RULOS_UART1_TX_PIN,
        .tx_altfunc = GPIO_AF1_USART2,
        .tx_dma_chan = DMA1_Channel2,
        .tx_dma_irqn = DMA1_Channel2_3_IRQn,
        .tx_dma_request = DMA_REQUEST_USART2_TX,
#endif
    },
#ifdef USART3
    {
      // rulos uart 2
        .instance = USART3,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,

        .rx_pin = RULOS_UART2_RX_PIN,
        .rx_altfunc = GPIO_AF4_USART3,
        .rx_dma_instance = DMA2,
        .rx_dma_channel = LL_DMA_CHANNEL_1,
        .rx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .rx_dma_request = DMA_REQUEST_USART3_RX,

        .tx_pin = RULOS_UART2_TX_PIN,
        .tx_altfunc = GPIO_AF4_USART3,
#ifndef UART_SURRENDER_DMA1_CHAN2_3
        .tx_dma_chan = DMA1_Channel3,
        .tx_dma_irqn = DMA1_Channel2_3_IRQn,
        .tx_dma_request = DMA_REQUEST_USART3_TX,
#endif
    },
#endif
#ifdef USART4
    {
      // rulos uart 3
        .instance = USART4,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_pin = RULOS_UART3_RX_PIN,
        .rx_altfunc = GPIO_AF4_USART4,
        .rx_dma_instance = DMA2,
        .rx_dma_channel = LL_DMA_CHANNEL_2,
        .rx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .rx_dma_request = DMA_REQUEST_USART4_RX,

        .tx_pin = RULOS_UART3_TX_PIN,
        .tx_altfunc = GPIO_AF4_USART4,
        .tx_dma_chan = DMA1_Channel6,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART4_TX,
    },
#endif
#ifdef USART5
    {
      // rulos uart 4
        .instance = USART5,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,

        .rx_pin = RULOS_UART4_RX_PIN,
#define GPIO_B1 123
#if RULOS_UART4_RX_PIN == GPIO_B1
        .rx_altfunc = GPIO_AF8_USART5,
#else
        .rx_altfunc = GPIO_AF3_USART5,
#endif
#undef GPIO_B1
        .rx_dma_instance = DMA2,
        .rx_dma_channel = LL_DMA_CHANNEL_3,
        .rx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .rx_dma_request = DMA_REQUEST_USART5_RX,

        .tx_pin = RULOS_UART4_TX_PIN,
        .rx_altfunc = GPIO_AF3_USART5,
        .tx_dma_chan = DMA1_Channel7,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART5_TX,
    },
#endif
#ifdef USART6
    {
      // rulos uart 5
        .instance = USART6,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_pin = RULOS_UART5_RX_PIN,
        .rx_altfunc = GPIO_AF3_USART6,
        .rx_dma_instance = DMA2,
        .rx_dma_channel = LL_DMA_CHANNEL_4,
        .rx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .rx_dma_request = DMA_REQUEST_USART6_RX,

        .tx_pin = RULOS_UART5_TX_PIN,
        .tx_altfunc = GPIO_AF3_USART6,
        .tx_dma_chan = DMA2_Channel5,
        .tx_dma_irqn = DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn,
        .tx_dma_request = DMA_REQUEST_USART6_TX,
    },
#endif
};

///////////// stm32g4

#elif defined(RULOS_ARM_stm32g4)

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

void DMA1_Channel1_IRQHandler() {
  dispatch_tx_dma(0);
}

void DMA1_Channel2_IRQHandler() {
  on_rx_dma_interrupt(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF7_USART1,
        .rx_dma_channel = LL_DMA_CHANNEL_2,
        .rx_dma_irqn = DMA1_Channel2_IRQn,
        .rx_dma_request = DMA_REQUEST_USART1_RX,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF7_USART1,
        .tx_dma_chan = DMA1_Channel1,
        .tx_dma_irqn = DMA1_Channel1_IRQn,
        .tx_dma_request = DMA_REQUEST_USART1_TX,
    },
};

/////////////////////////////////////////

#else
#error "Tell the UART code about your chip's UART."
#include <stophere>
#endif

// Eventually this should be conditional depending on what kind of
// chip you have
#define NUM_UARTS (sizeof(stm32_uart_config) / sizeof(stm32_uart_config[0]))
static stm32_uart_t g_stm32_uarts[NUM_UARTS] = {};

#ifdef USE_RX_DMA_FOR_CHIP
#define USART_USING_RX_DMA(config) ((config)->rx_dma_instance != NULL)
#else
#define USART_USING_RX_DMA(config) (false)
#endif

///////////////////// reception /////////////////////////////

static void on_rx_buffer_full(uint8_t uart_id, stm32_uart_t *u,
                              const stm32_uart_config_t *c);

//// Receiving: DMA Version

#if USE_RX_DMA_FOR_CHIP

static void hal_uart_start_rx_dma(stm32_uart_t *u,
                                  const stm32_uart_config_t *config) {
  LL_DMA_DisableChannel(config->rx_dma_instance, config->rx_dma_channel);

  // Enable DMA RX interrupts
  NVIC_SetPriority(config->rx_dma_irqn, 0);
  NVIC_EnableIRQ(config->rx_dma_irqn);

  LL_DMA_ClearFlag_TC(config->rx_dma_instance, config->rx_dma_channel);
  LL_DMA_ConfigTransfer(config->rx_dma_instance, config->rx_dma_channel,
                        LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
                            LL_DMA_PRIORITY_MEDIUM | LL_DMA_MODE_NORMAL |
                            LL_DMA_PERIPH_NOINCREMENT |
                            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE |
                            LL_DMA_MDATAALIGN_BYTE);

  LL_DMA_ConfigAddresses(
      config->rx_dma_instance, config->rx_dma_channel,
      LL_USART_DMA_GetRegAddr(config->instance, LL_USART_DMA_REG_DATA_RECEIVE),
      (uint32_t)u->rx_curr_base_buf,
      LL_DMA_GetDataTransferDirection(config->rx_dma_instance,
                                      config->rx_dma_channel));
  LL_DMA_SetDataLength(config->rx_dma_instance, config->rx_dma_channel,
                       u->rx_half_buflen);
  LL_DMA_SetPeriphRequest(config->rx_dma_instance, config->rx_dma_channel,
                          config->rx_dma_request);
  LL_DMA_EnableIT_TC(config->rx_dma_instance, config->rx_dma_channel);
  LL_USART_EnableDMAReq_RX(config->instance);
  LL_DMA_EnableChannel(config->rx_dma_instance, config->rx_dma_channel);
}

// Called when a DMA interrupt arrives for a UART. Determine if it is a
// completion interrupt, and if so, send an upcall.
static void on_rx_dma_interrupt(uint8_t uart_id) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];

  if (!u->initted) {
    return;
  }

  u->tot_ints++;

  if (LL_DMA_IsActiveFlag_TC(config->rx_dma_instance, config->rx_dma_channel)) {
    LL_DMA_ClearFlag_TC(config->rx_dma_instance, config->rx_dma_channel);
    u->tot_rx_bytes += u->rx_half_buflen;
    on_rx_buffer_full(uart_id, u, config);
  }
}

static size_t get_rx_stored_chars_dma(uint8_t uart_id, stm32_uart_t *u,
                                      const stm32_uart_config_t *c) {
  // Disable DMA
  LL_DMA_DisableChannel(c->rx_dma_instance, c->rx_dma_channel);

  // Determine how many bytes are left to read, and thus how many were read
  const size_t free_space =
      LL_DMA_GetDataLength(c->rx_dma_instance, c->rx_dma_channel);
  const size_t rx_chars = u->rx_half_buflen - free_space;
  u->tot_rx_bytes += rx_chars;
  return rx_chars;
}

#else  // USE_RX_DMA_FOR_CHIP

static void hal_uart_start_rx_dma(stm32_uart_t *u,
                                  const stm32_uart_config_t *config) {
}

static size_t get_rx_stored_chars_dma(uint8_t uart_id, stm32_uart_t *u,
                                      const stm32_uart_config_t *c) {
  return 0;
}

#endif  // USE_RX_DMA_FOR_CHIP

//// Receiving: Per-Char Interrupt Version

static void hal_uart_start_rx_interrupt(stm32_uart_t *u,
                                        const stm32_uart_config_t *config) {
  LL_USART_EnableIT_RXNE(config->instance);
}

static void receive_char_int(uint8_t uart_id, stm32_uart_t *u,
                             const stm32_uart_config_t *config, char c) {
  // store the character
  u->rx_curr_base_buf[u->rx_bytes_stored] = c;
  u->rx_bytes_stored++;
  u->tot_rx_bytes++;

  // if the half-buffer has filled, make an upcall
  if (u->rx_bytes_stored == u->rx_half_buflen) {
    on_rx_buffer_full(uart_id, u, config);
  }
}

static size_t get_rx_stored_chars_int(uint8_t uart_id, stm32_uart_t *u,
                                      const stm32_uart_config_t *c) {
  return u->rx_bytes_stored;
}

//// Receiving: shared between DMA and Per-Char

static char *switch_rx_buffers(stm32_uart_t *u) {
  char *oldbuf = u->rx_curr_base_buf;
  if (oldbuf == u->rx_buf) {
    u->rx_curr_base_buf += u->rx_half_buflen;
  } else {
    u->rx_curr_base_buf = u->rx_buf;
  }
  u->rx_bytes_stored = 0;
  u->rx_data_ready = false;
  return oldbuf;
}

static void launch_next_rx(stm32_uart_t *u, const stm32_uart_config_t *config) {
  // launch a read on that buffer
  if (USART_USING_RX_DMA(config)) {
    hal_uart_start_rx_dma(u, config);
  } else {
    hal_uart_start_rx_interrupt(u, config);
  }
}

static void rx_send_up(uint8_t uart_id, stm32_uart_t *u, char *buf,
                       size_t len) {
  assert(u->rx_cb_ready);
  if (len > 0) {
    u->rx_cb_ready = false;
    u->rx_cb(uart_id, u->user_data, buf, len);
    if (len > u->max_chars_per_rx_batch) {
      u->max_chars_per_rx_batch = len;
    }
  }
}

// Maybe flush the RX buffer -- if there's data, and the upper layer is ready to
// receive
static void maybe_flush_rx_buf(uint8_t uart_id, stm32_uart_t *u,
                               const stm32_uart_config_t *c) {
  if (!u->rx_cb_ready) {
    return;
  }

  if (!u->rx_data_ready) {
    return;
  }

  size_t len = 0;
  if (USART_USING_RX_DMA(c)) {
    len = get_rx_stored_chars_dma(uart_id, u, c);
    //LOG("dma uart %d: got %u", uart_id, len);
  } else {
    len = get_rx_stored_chars_int(uart_id, u, c);
    //LOG("int uart %d: got %u", uart_id, len);
  }

  char *oldbuf = switch_rx_buffers(u);
  launch_next_rx(u, c);
  rx_send_up(uart_id, u, oldbuf, len);
}

static void on_rx_buffer_full(uint8_t uart_id, stm32_uart_t *u,
                              const stm32_uart_config_t *c) {
  // RX buffer is full, so we better be done processing the other one! Otherwise
  // we have to drop it.
  if (!u->rx_cb_ready) {
    // uh oh! we can't send the buffer up because the previous upcall is still
    // outstanding. no choice but to lose the data.
    u->dropped_rx_bytes += u->rx_half_buflen;
    u->rx_bytes_stored = 0;
  } else {
    char *oldbuf = switch_rx_buffers(u);
    rx_send_up(uart_id, u, oldbuf, u->rx_half_buflen);
  }
  launch_next_rx(u, c);
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb, void *buf,
                       size_t buflen) {
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
  u->rx_buf = buf;
  u->rx_curr_base_buf = u->rx_buf;
  u->rx_half_buflen = buflen / 2;
  u->rx_cb_ready = true;

  // If the hardware supports a UART FIFO, enable it
#ifdef LL_USART_ISR_RXNE_RXFNE
  LL_USART_EnableFIFO(config->instance);
#endif

  // clear spurious char that might be sitting in the rx register
  LL_USART_ReceiveData8(config->instance);

  // Enable the idle interrupt so we can flush rx buffers upwards when each
  // message ends
  LL_USART_ClearFlag_IDLE(config->instance);
  LL_USART_EnableIT_IDLE(config->instance);
  launch_next_rx(u, config);
}

static void on_usart_interrupt(uint8_t uart_id) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *c = &stm32_uart_config[uart_id];
  if (!u->initted) {
    return;
  }

  u->tot_ints++;

  // if a character arrived, add it to the rx buffer
  int num_read = 0;
  while (LL_USART_IsActiveFlag_RXNE(c->instance)) {
    // note: we have to read the character whether or not we send it anywhere;
    // reading the char is what clears the interrupt
    num_read++;
    receive_char_int(uart_id, u, c, LL_USART_ReceiveData8(c->instance));
  }

  // If we read anything, update some statistics
  if (num_read > 0) {
    if (num_read > u->max_chars_per_rx_isr) {
      u->max_chars_per_rx_isr = num_read;
    }
    if (u->min_chars_per_rx_isr == 0 || num_read < u->min_chars_per_rx_isr) {
      u->min_chars_per_rx_isr = num_read;
    }
  }

  // on idle interrupt, flush the rx buffers upwards
  if (LL_USART_IsActiveFlag_IDLE(c->instance)) {
    LL_USART_ClearFlag_IDLE(c->instance);
    u->idle_ints++;
    u->rx_data_ready = true;
    maybe_flush_rx_buf(uart_id, u, c);
  }

  // handle RX errors by clearing flags and updating statistics
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
  if (LL_USART_IsActiveFlag_ORE(c->instance)) {
    LL_USART_ClearFlag_ORE(c->instance);
    u->overruns++;
  }

  // dispatch the rest of the interrupt handling through the HAL
  HAL_UART_IRQHandler(&u->hal_uart_handle);
}

void hal_uart_rx_cb_done(uint8_t uart_id) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *c = &stm32_uart_config[uart_id];

  rulos_irq_state_t irq = hal_start_atomic();
  assert(!u->rx_cb_ready);
  u->rx_cb_ready = true;
  maybe_flush_rx_buf(uart_id, u, c);
  hal_end_atomic(irq);
}

/////// transmission

static void dispatch_tx_dma(uint8_t uart_id) {
  const stm32_uart_t *u = &g_stm32_uarts[uart_id];
  if (!u->initted) {
    return;
  }

  HAL_DMA_IRQHandler(u->hal_uart_handle.hdmatx);
}

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
    if (HAL_UART_Transmit_DMA(&uart->hal_uart_handle, (uint8_t *)buf, len) !=
        HAL_OK) {
      __builtin_trap();
    }
    uart->tot_tx_bytes += len;
    if (len > uart->max_chars_per_tx_batch) {
      uart->max_chars_per_tx_batch = len;
    }
  }
}

// Callback called when TX is complete. This overrides a weak symbol in the HAL
// implementation.
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *hal_uart_handle) {
  for (unsigned int i = 0; i < NUM_UARTS; i++) {
    if (&g_stm32_uarts[i].hal_uart_handle == hal_uart_handle) {
      maybe_launch_next_tx(&g_stm32_uarts[i]);
    }
  }
}

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

  if (rx) {
    GPIO_InitStruct.Pin = config->rx_pin.pin;
#if !defined(RULOS_ARM_stm32f1)
    GPIO_InitStruct.Alternate = config->rx_altfunc;
#endif
    HAL_GPIO_Init(config->rx_pin.port, &GPIO_InitStruct);
  } else {
    GPIO_InitStruct.Pin = config->tx_pin.pin;
#if !defined(RULOS_ARM_stm32f1)
    GPIO_InitStruct.Alternate = config->tx_altfunc;
#endif
    HAL_GPIO_Init(config->tx_pin.port, &GPIO_InitStruct);
  }
}

void hal_uart_init(uint8_t uart_id, uint32_t baud,
                   void *user_data /* for both rx and tx upcalls */,
                   size_t *max_tx_len /* OUT */) {
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

void hal_uart_log_stats(uint8_t uart_id) {
  assert(uart_id < NUM_UARTS);
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  (void)u;

  LOG("stats for UART %u", uart_id);
  LOG(" total rx: %lu", u->tot_rx_bytes);
  LOG(" total tx: %lu", u->tot_tx_bytes);
  LOG(" frame_errors: %lu", u->frame_errors);
  LOG(" parity_errors: %lu", u->parity_errors);
  LOG(" noise_errors: %lu", u->noise_errors);
  LOG(" overruns: %lu", u->overruns);
  LOG(" dropped rx bytes: %lu", u->dropped_rx_bytes);
#ifdef LL_USART_ISR_RXNE_RXFNE
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
  (void)config;
  LOG(" rx fifo: %lu", LL_USART_GetRXFIFOThreshold(config->instance));
#endif
  LOG(" min isr chars: %u", u->min_chars_per_rx_isr);
  LOG(" max isr chars: %u", u->max_chars_per_rx_isr);
}
