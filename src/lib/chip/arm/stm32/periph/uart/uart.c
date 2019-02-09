/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

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

#if defined(STM32F103xB)

#define rUART1_TX_PORT GPIOA
#define rUART1_TX_PIN GPIO_PIN_9
// CLK_ENABLE must match port above
#define rUART1_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

// RX, TX can also be put on B7, B6 as remap pins
#define rUART1_RX_PORT GPIOA
#define rUART1_RX_PIN GPIO_PIN_10
// CLK_ENABLE must match port above
#define rUART1_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define rUART1_DMA_TXCHAN DMA1_Channel7
#define rUART1_DMA_RXCHAN DMA1_Channel6
// CLK_ENABLE must match DMA unit above
#define rUART1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()

#else
#error "Tell the UART code about your chip's UART."
#endif

typedef struct {
  // UART handle from STM32's HAL
  UART_HandleTypeDef UartHandle;

  // Transmit and receive DMA buffers
  uint8_t txbuff[UART_TXBUF_SIZE];
  uint8_t rxbuff[UART_RXBUF_SIZE];
} uart_t;

// Eventually this should be conditional depending on what kind of
// chip you have
#define NUM_UARTS 1
static uart_t g_uarts[NUM_UARTS] = {};

void UART_IRQHandler(void) {}

static void stm32_uart_init(uint8_t uart_id, uint32_t baud, r_bool stop2) {
  uart_t *uart = &g_uarts[uart_id];

  // Note: HAL uarts are numbered starting from 0.
  // STM32 UARTs are numbered starting from 1.

  switch (uart_id) {
    case 0:
      // Enable peripherals and GPIO clocks
      rUART1_TX_GPIO_CLK_ENABLE();
      rUART1_RX_GPIO_CLK_ENABLE();
      __HAL_RCC_AFIO_CLK_ENABLE();

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
        HAL_GPIO_Init(rUART1_TX_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = rUART1_RX_PIN;
        HAL_GPIO_Init(rUART1_RX_PORT, &GPIO_InitStruct);
      }

      // Configure UART HAL
      uart->UartHandle.Instance = USART1;
      uart->UartHandle.Init.BaudRate = baud;
      uart->UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
      uart->UartHandle.Init.Parity = UART_PARITY_NONE;
      uart->UartHandle.Init.StopBits =
          stop2 ? UART_STOPBITS_2 : UART_STOPBITS_1;
      uart->UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
      uart->UartHandle.Init.Mode = UART_MODE_TX_RX;
      if (HAL_UART_Init(&uart->UartHandle) != HAL_OK) {
        __builtin_trap();
      }
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

static void arm_uart_sync_send_bytes_by_id(uint8_t uart_id, const uint8_t *s,
                                           uint32_t len) {
  uart_t *uart = &g_uarts[uart_id];

  while (len > 0) {
    // Wait until a previous transmission has completed, if any
    while (HAL_UART_GetState(&uart->UartHandle) != HAL_UART_STATE_READY) {
      __WFI();
    }

    HAL_UART_Transmit(&uart->UartHandle, s, len, HAL_MAX_DELAY);
    len = 0;
  }
}

void arm_uart_sync_send_by_id(uint8_t uart_id, const char *s) {
  arm_uart_sync_send_bytes_by_id(uart_id, (uint8_t *)s, strlen(s));
}

void hal_uart_sync_send(HalUart *handler, const char *s) {
  arm_uart_sync_send_by_id(handler->uart_id, s);
}

void hal_uart_sync_send_bytes(HalUart *handler, const uint8_t *s, uint8_t len) {
  arm_uart_sync_send_bytes_by_id(handler->uart_id, s, len);
}

void arm_log_flush() {}
