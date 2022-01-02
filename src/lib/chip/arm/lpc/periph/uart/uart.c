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
#include "core/logging.h"

//////////////////////////////////////////////////////////////////////////////

/* Transmit and receive ring buffer sizes */
#ifndef UART_SRB_SIZE
#define UART_SRB_SIZE 128 /* Send */
#endif

#ifndef UART_RRB_SIZE
#define UART_RRB_SIZE 32 /* Receive */
#endif

typedef struct {
  /* Transmit and receive ring buffers */
  LPC_USART_T *LPC_UART;
  RINGBUFF_T rxring;
  RINGBUFF_T txring;
  uint8_t rxbuff[UART_RRB_SIZE];
  uint8_t txbuff[UART_SRB_SIZE];
} uart_t;

// Eventually this should be conditional depending on what kind of
// chip you have
#define NUM_UARTS 1
static uart_t g_uarts[NUM_UARTS] = {
    {LPC_USART, {0}, {0}, {0}, {0}},
};

static void handle_irq_by_uart_id(uint8_t uart_id) {
  uart_t *uart = &g_uarts[uart_id];
  Chip_UART_IRQRBHandler(uart->LPC_UART, &uart->rxring, &uart->txring);
}

void UART_IRQHandler(void) {
  // dispatch to uart id 0
  handle_irq_by_uart_id(0);
}

static void init_pins() {
#if defined(CHIP_LPC11CXX)
  gpio_iocon(GPIO1_06, IOCON_FUNC1 | IOCON_MODE_INACT);
  gpio_iocon(GPIO1_07, IOCON_FUNC1 | IOCON_MODE_INACT);
#else
#error "Chip-specific UART code needs help"
#endif
}

void hal_uart_init(HalUart *hal_uart, uint32_t baud, uint8_t uart_id) {
  assert(uart_id < NUM_UARTS);

  uart_t *uart = &g_uarts[uart_id];
  init_pins();
  Chip_UART_Init(uart->LPC_UART);
  Chip_UART_SetBaud(uart->LPC_UART, baud);

  uint32_t config_data = UART_LCR_WLEN8;  // 8 bits
  config_data |= UART_LCR_SBS_1BIT;       // 1 stop bit

  Chip_UART_ConfigData(uart->LPC_UART, config_data);

  // Enable hardware fifos with a receive trigger level of 8 bytes
  Chip_UART_SetupFIFOS(uart->LPC_UART, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));

  // Init ring buffers
  RingBuffer_Init(&uart->rxring, uart->rxbuff, 1, UART_RRB_SIZE);
  RingBuffer_Init(&uart->txring, uart->txbuff, 1, UART_SRB_SIZE);

  // Enable receive data and line status interrupt
  Chip_UART_IntEnable(uart->LPC_UART, (UART_IER_RBRINT | UART_IER_RLSINT));

  // Set interrupt priorities
  NVIC_SetPriority(UART0_IRQn, 1);
  NVIC_EnableIRQ(UART0_IRQn);

  hal_uart->uart_id = uart_id;
}

static int uart_enqueue_bytes_by_id(const uint8_t uart_id, const char *data,
                                    uint32_t len) {
  uart_t *uart = &g_uarts[uart_id];
  return Chip_UART_SendRB(uart->LPC_UART, &uart->txring, data, len);
}

int uart_enqueue_by_id(const uint8_t uart_id, const char *s) {
  return uart_enqueue_bytes_by_id(uart_id, s, strlen(s));
}

static int uart_enqueue_bytes(HalUart *uart, const char *data, uint32_t len) {
  return uart_enqueue_bytes_by_id(uart->uart_id, data, len);
}

int uart_enqueue(HalUart *uart, char *s) {
  return uart_enqueue_bytes(uart, s, strlen(s));
}

void uart_sync_send_bytes_by_id(uint8_t uart_id, const char *s, uint32_t len) {
  while (1) {
    int sent = uart_enqueue_bytes_by_id(uart_id, s, len);

    assert(sent <= len);

    s += sent;
    len -= sent;

    if (len > 0) {
      __WFI();
    } else {
      return;
    }
  }
}

void uart_sync_send_by_id(uint8_t uart_id, const char *s) {
  uart_sync_send_bytes_by_id(uart_id, s, strlen(s));
}

void hal_uart_sync_send_bytes(HalUart *uart, const char *s, uint8_t len) {
  uart_sync_send_bytes_by_id(uart->uart_id, s, len);
}

void hal_uart_sync_send(HalUart *uart, const char *s) {
  uart_sync_send_by_id(uart->uart_id, s);
}
