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

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/logging.h"

//////////////////////////////////////////////////////////////////////////////

/* Transmit and receive buffer sizes */
#ifndef UART_TXBUF_SIZE
#define UART_TXBUF_SIZE 128
#endif

#ifndef UART_RXBUF_SIZE
#define UART_RXBUF_SIZE 128 /* Receive */
#endif

#if defined(RULOS_ARM_stm32f0)

#define rUART1_TX_PORT GPIOA
#define rUART1_TX_PIN GPIO_PIN_9
// CLK_ENABLE must match port above
#define rUART1_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

// RX, TX can also be put on B7, B6 as remap pins
#define rUART1_RX_PORT GPIOA
#define rUART1_RX_PIN GPIO_PIN_10
// CLK_ENABLE must match port above
#define rUART1_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define rUART1_GPIO_ALTFUNC GPIO_AF1_USART1

#define rUART1_DMA_TX_CHAN DMA1_Channel2
#define rUART1_DMA_TX_IRQn DMA1_Channel2_3_IRQn
#define rUART1_DMA_TX_IRQHandler DMA1_Channel2_3_IRQHandler

//#define rUART1_DMA_RX_CHAN DMA1_Channel3
//#define rUART1_DMA_RX_IRQn DMA1_Channel2_3_IRQn
//#define rUART1_DMA_RX_IRQHandler DMA1_Channel2_3_IRQHandler

// CLK_ENABLE must match DMA unit above
#define rUART1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()

#elif defined(RULOS_ARM_stm32f1)

#define rUART1_TX_PORT GPIOA
#define rUART1_TX_PIN GPIO_PIN_9
// CLK_ENABLE must match port above
#define rUART1_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

// RX, TX can also be put on B7, B6 as remap pins
#define rUART1_RX_PORT GPIOA
#define rUART1_RX_PIN GPIO_PIN_10
// CLK_ENABLE must match port above
#define rUART1_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define rUART1_DMA_TX_CHAN DMA1_Channel4
#define rUART1_DMA_TX_IRQn DMA1_Channel4_IRQn
#define rUART1_DMA_TX_IRQHandler DMA1_Channel4_IRQHandler

//#define rUART1_DMA_RX_CHAN DMA1_Channel5
//#define rUART1_DMA_RX_IRQn DMA1_Channel5_IRQn
//#define rUART1_DMA_RX_IRQHandler DMA1_Channel5_IRQHandler

// CLK_ENABLE must match DMA unit above
#define rUART1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()

#elif defined(RULOS_ARM_stm32f3)

#define rUART1_TX_PORT GPIOA
#define rUART1_TX_PIN GPIO_PIN_9
// CLK_ENABLE must match port above
#define rUART1_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

// RX, TX can also be put on B7, B6 or C5, C4
#define rUART1_RX_PORT GPIOA
#define rUART1_RX_PIN GPIO_PIN_10
// CLK_ENABLE must match port above
#define rUART1_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define rUART1_GPIO_ALTFUNC GPIO_AF7_USART1

#define rUART1_DMA_TX_CHAN DMA1_Channel4
#define rUART1_DMA_TX_IRQn DMA1_Channel4_IRQn
#define rUART1_DMA_TX_IRQHandler DMA1_Channel4_IRQHandler

//#define rUART1_DMA_RX_CHAN DMA1_Channel5
//#define rUART1_DMA_RX_IRQn DMA1_Channel5_IRQn
//#define rUART1_DMA_RX_IRQHandler DMA1_Channel5_IRQHandler

// CLK_ENABLE must match DMA unit above
#define rUART1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()

#else
#error "Tell the UART code about your chip's UART."
#include <stophere>
#endif

typedef struct {
  // UART handle from STM32's HAL
  UART_HandleTypeDef hal_uart_handle;

  // Transmit and receive DMA buffers and DMA handles
  DMA_HandleTypeDef hal_dma_tx_handle;
  DMA_HandleTypeDef hal_dma_rx_handle;
  uint8_t txbuf[UART_TXBUF_SIZE];
  uint8_t rxbuf[UART_RXBUF_SIZE];
} uart_t;

// Eventually this should be conditional depending on what kind of
// chip you have
#define NUM_UARTS 1
static uart_t g_uarts[NUM_UARTS] = {};

// If the DMA RX and TX events share an interrupt, call both from the
// handler.
void rUART1_DMA_TX_IRQHandler(void) {
  HAL_DMA_IRQHandler(g_uarts[0].hal_uart_handle.hdmatx);
}

void USART1_IRQHandler(void) {
  HAL_UART_IRQHandler(&g_uarts[0].hal_uart_handle);
}

static void stm32_uart_init(uint8_t uart_id, uint32_t baud, r_bool stop2) {
  uart_t *uart = &g_uarts[uart_id];

  // Note: HAL uarts are numbered starting from 0.
  // STM32 UARTs are numbered starting from 1.

  switch (uart_id) {
    case 0:
      // Enable peripherals and GPIO clocks
      rUART1_TX_GPIO_CLK_ENABLE();
      rUART1_RX_GPIO_CLK_ENABLE();
#ifdef __HAL_RCC_AFIO_CLK_ENABLE
      __HAL_RCC_AFIO_CLK_ENABLE();
#endif

      // Enable USART clock
      __HAL_RCC_USART1_CLK_ENABLE();

      // Enable DMA clock
      rUART1_DMA_CLK_ENABLE();

      // Configure GPIO peripherals
      {
        GPIO_InitTypeDef GPIO_InitStruct;
        GPIO_InitStruct.Pin = rUART1_TX_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#ifdef rUART1_GPIO_ALTFUNC
        GPIO_InitStruct.Alternate = rUART1_GPIO_ALTFUNC;
#endif
        HAL_GPIO_Init(rUART1_TX_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = rUART1_RX_PIN;
        HAL_GPIO_Init(rUART1_RX_PORT, &GPIO_InitStruct);
      }

      // Configure UART HAL
      uart->hal_uart_handle.Instance = USART1;
      uart->hal_uart_handle.Init.BaudRate = baud;
      uart->hal_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
      uart->hal_uart_handle.Init.Parity = UART_PARITY_NONE;
      uart->hal_uart_handle.Init.StopBits =
          stop2 ? UART_STOPBITS_2 : UART_STOPBITS_1;
      uart->hal_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
      uart->hal_uart_handle.Init.Mode = UART_MODE_TX_RX;
      if (HAL_UART_Init(&uart->hal_uart_handle) != HAL_OK) {
        __builtin_trap();
      }

      // Configure the DMA controller for TX
      uart->hal_dma_tx_handle.Instance = rUART1_DMA_TX_CHAN;
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

#if 0
      // NOTE: No longer using DMA for RX.
      // Configure the DMA handler for reception process */
      uart->hal_dma_rx_handle.Instance = rUART1_DMA_RX_CHAN;
      uart->hal_dma_rx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
      uart->hal_dma_rx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
      uart->hal_dma_rx_handle.Init.MemInc = DMA_MINC_ENABLE;
      uart->hal_dma_rx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
      uart->hal_dma_rx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
      uart->hal_dma_rx_handle.Init.Mode = DMA_NORMAL;
      uart->hal_dma_rx_handle.Init.Priority = DMA_PRIORITY_HIGH;
      if (HAL_DMA_Init(&uart->hal_dma_rx_handle) != HAL_OK) {
        __builtin_trap();
      }

      __HAL_LINKDMA(&uart->hal_uart_handle, hdmarx, uart->hal_dma_rx_handle);
#endif

      // Set up interrupts
      HAL_NVIC_SetPriority(rUART1_DMA_TX_IRQn, 0, 1);
      HAL_NVIC_EnableIRQ(rUART1_DMA_TX_IRQn);

#if 0
      // NOTE: No longer using DMA for RX.
      /* NVIC configuration for DMA transfer complete interrupt (USARTx_RX) */
      HAL_NVIC_SetPriority(rUART1_DMA_RX_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(rUART1_DMA_RX_IRQn);
#endif

      /* NVIC for USART, to catch the TX complete */
      HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
      HAL_NVIC_EnableIRQ(USART1_IRQn);

      break;

    default:
      // Please add a second UART to this case statement!
      __builtin_trap();
  }
}

void hal_uart_init(HalUart *hal_uart, uint32_t baud, r_bool stop2,
                   uint8_t uart_id) {
  assert(uart_id < NUM_UARTS);
  hal_uart->uart_id = uart_id;
  stm32_uart_init(uart_id, baud, stop2);
}

// This function will try to immediately unblock the sender as long as
// the number of bytes being transmitted is less than the size of the
// txbuf and the previous transmission has completed. If the previous
// transmission has not yet completed or a single message is larger
// than UART_TXBUF_SIZE, this function will block. If you expect to
// send single messages larger than the default txbuf size, consider
// increasing it using ARM_CFLAGS += -DUART_TXBUF_SIZE=nnnn in your
// Makefile.
static void arm_uart_sync_send_bytes_by_id(uint8_t uart_id, const void *s,
                                           uint32_t len) {
  uart_t *uart = &g_uarts[uart_id];

  while (len > 0) {
    // Wait until a previous transmission has completed, if any
    while (HAL_UART_GetState(&uart->hal_uart_handle) != HAL_UART_STATE_READY) {
      __WFI();
    }

    // Copy as much of the message as will fit into the DMA buffer
    uint32_t num_copied = len;
    if (num_copied > UART_TXBUF_SIZE) {
      num_copied = UART_TXBUF_SIZE;
    }
    memcpy(uart->txbuf, s, num_copied);

    // Start the transfer
    if (HAL_UART_Transmit_DMA(&uart->hal_uart_handle, uart->txbuf,
                              num_copied) != HAL_OK) {
      __builtin_trap();
    }
    len -= num_copied;
    s += num_copied;
  }
}

void arm_uart_sync_send_by_id(uint8_t uart_id, const char *s) {
  arm_uart_sync_send_bytes_by_id(uart_id, s, strlen(s));
}

void hal_uart_sync_send(HalUart *handler, const char *s) {
  arm_uart_sync_send_by_id(handler->uart_id, s);
}

void hal_uart_sync_send_bytes(HalUart *handler, const void *s, uint8_t len) {
  arm_uart_sync_send_bytes_by_id(handler->uart_id, s, len);
}

void arm_log_flush() {}
