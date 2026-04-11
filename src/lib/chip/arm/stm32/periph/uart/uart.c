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

#include "core/dma.h"

/*
 * USE_RX_DMA_FOR_CHIP: chip has enough DMA channels to dedicate one to
 * UART RX (G0/G4). Smaller F-family chips use per-char RXNE interrupts
 * because their fixed-map DMA controllers don't have a spare channel
 * for every UART.
 */
#if defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32g4)
#define USE_RX_DMA_FOR_CHIP 1
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

  // RULOS DMA channel handles (allocated in hal_uart_init). rx_dma_ch
  // only exists on families that use DMA for RX (USE_RX_DMA_FOR_CHIP =
  // G0/G4); on F0/F1/F3 the field is compiled out.
  rulos_dma_channel_t *tx_dma_ch;
#if USE_RX_DMA_FOR_CHIP
  rulos_dma_channel_t *rx_dma_ch;
#endif
  // Base address of the peripheral's TX data register. Cached so the
  // tc_callback can re-arm the next transfer from ISR context.
  volatile void *usart_tdr;

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

  // tx
  gpio_pin_t tx_pin;
  uint32_t tx_altfunc;

  // Logical DMA request identifiers; the RULOS DMA core translates
  // these to hardware channel assignments at init time. rx_dma_req
  // is only meaningful on families that use DMA for RX.
  rulos_dma_request_t tx_dma_req;
#if USE_RX_DMA_FOR_CHIP
  rulos_dma_request_t rx_dma_req;
#endif
} stm32_uart_config_t;

static void on_usart_interrupt(uint8_t uart_id);
static void config_gpio(const stm32_uart_config_t *config, bool rx);

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



///////////// stm32c0

#if defined(RULOS_ARM_stm32c0)

// DMA IRQ handlers are owned by the RULOS DMA core (core/dma.c).
void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF1_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF1_USART1,

        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
    },
};

///////////// stm32f0

#elif defined(RULOS_ARM_stm32f0)

// DMA IRQ handlers are owned by the RULOS DMA core (core/dma.c).
void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF1_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF1_USART1,

        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
    },
};

///////////// stm32f1

#elif defined(RULOS_ARM_stm32f1)

// DMA IRQ handlers are owned by the RULOS DMA core (core/dma.c).
void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,
        .rx_pin = RULOS_UART0_RX_PIN,
        .tx_pin = RULOS_UART0_TX_PIN,

        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
    },
};

///////////// stm32f3

#elif defined(RULOS_ARM_stm32f3)

// DMA IRQ handlers are owned by the RULOS DMA core (core/dma.c).
void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF7_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF7_USART1,

        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
    },
};

///////////// stm32g0

#elif defined(RULOS_ARM_stm32g0)

// G0 DMA channels are allocated dynamically by the RULOS DMA core
// (core/dma.c). No more hand-maintained (uart, dma channel) mapping
// tables or UART_SURRENDER_* hacks. The allocator picks the first
// free DMA channel when each UART's hal_uart_init runs, and the
// DMAMUX routes the peripheral request there.

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

// USART peripheral IRQ handlers below handle RXNE / IDLE / error flags
// (see on_usart_interrupt). They are unrelated to DMA; DMA IRQs are
// owned by core/dma.c. On G0B1, USART2 shares its IRQ line with LPUART2
// and USART3/4/5/6 share with LPUART1, so the handlers dispatch to
// all of them in one go -- on_usart_interrupt early-exits for UARTs
// that aren't initialized.

#if defined(STM32G0B1xx)
#define USART2_IRQn USART2_LPUART2_IRQn
void USART2_LPUART2_IRQHandler() {
  on_usart_interrupt(1);
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

        .tx_pin = RULOS_UART0_TX_PIN,
#define GPIO_A9 123
#if RULOS_UART0_TX_PIN == GPIO_A9
        .tx_altfunc = GPIO_AF1_USART1,
#else
        .tx_altfunc = GPIO_AF0_USART1,
#endif
#undef GPIO_A9

        .rx_dma_req = RULOS_DMA_REQ_USART1_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
    },
    {
      // rulos uart 1
        .instance = USART2,
        .instance_irqn = USART2_IRQn,
        .rx_pin = RULOS_UART1_RX_PIN,
        .rx_altfunc = GPIO_AF1_USART2,
        .tx_pin = RULOS_UART1_TX_PIN,
        .tx_altfunc = GPIO_AF1_USART2,
        .rx_dma_req = RULOS_DMA_REQ_USART2_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART2_TX,
    },
#ifdef USART3
    {
      // rulos uart 2
        .instance = USART3,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_pin = RULOS_UART2_RX_PIN,
        .rx_altfunc = GPIO_AF4_USART3,
        .tx_pin = RULOS_UART2_TX_PIN,
        .tx_altfunc = GPIO_AF4_USART3,
        .rx_dma_req = RULOS_DMA_REQ_USART3_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART3_TX,
    },
#endif
#ifdef USART4
    {
      // rulos uart 3
        .instance = USART4,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_pin = RULOS_UART3_RX_PIN,
        .rx_altfunc = GPIO_AF4_USART4,
        .tx_pin = RULOS_UART3_TX_PIN,
        .tx_altfunc = GPIO_AF4_USART4,
        .rx_dma_req = RULOS_DMA_REQ_USART4_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART4_TX,
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
        .tx_pin = RULOS_UART4_TX_PIN,
        .tx_altfunc = GPIO_AF3_USART5,
        .rx_dma_req = RULOS_DMA_REQ_USART5_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART5_TX,
    },
#endif
#ifdef USART6
    {
      // rulos uart 5
        .instance = USART6,
        .instance_irqn = USART3_4_5_6_LPUART1_IRQn,
        .rx_pin = RULOS_UART5_RX_PIN,
        .rx_altfunc = GPIO_AF3_USART6,
        .tx_pin = RULOS_UART5_TX_PIN,
        .tx_altfunc = GPIO_AF3_USART6,
        .rx_dma_req = RULOS_DMA_REQ_USART6_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART6_TX,
    },
#endif
};

///////////// stm32g4

#elif defined(RULOS_ARM_stm32g4)

void USART1_IRQHandler() {
  on_usart_interrupt(0);
}

// DMA IRQ handlers are owned by the RULOS DMA core (core/dma.c).

static const stm32_uart_config_t stm32_uart_config[] = {
    {
        .instance = USART1,
        .instance_irqn = USART1_IRQn,

        .rx_pin = RULOS_UART0_RX_PIN,
        .rx_altfunc = GPIO_AF7_USART1,

        .tx_pin = RULOS_UART0_TX_PIN,
        .tx_altfunc = GPIO_AF7_USART1,

        .rx_dma_req = RULOS_DMA_REQ_USART1_RX,
        .tx_dma_req = RULOS_DMA_REQ_USART1_TX,
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

/*
 * RX DMA is used when the chip family has enough DMA channels to
 * dedicate one to UART RX. On those families every configured UART
 * uses RX DMA (there's no "polled RX on a family with DMA" mixed
 * mode).
 */
#if USE_RX_DMA_FOR_CHIP
#define USART_USING_RX_DMA(config) (true)
#else
#define USART_USING_RX_DMA(config) (false)
#endif

///////////////////// reception /////////////////////////////

static void on_rx_buffer_full(uint8_t uart_id, stm32_uart_t *u,
                              const stm32_uart_config_t *c);

//// Receiving: DMA Version

#if USE_RX_DMA_FOR_CHIP

/*
 * The RX DMA channel is allocated once at hal_uart_init time; each
 * half-buffer restart just reprograms the addresses and re-starts
 * via rulos_dma_start.
 */
static void hal_uart_on_rx_dma_tc(void *user_data);

static void hal_uart_start_rx_dma(stm32_uart_t *u,
                                  const stm32_uart_config_t *config) {
  LL_USART_EnableDMAReq_RX(config->instance);
  rulos_dma_start(u->rx_dma_ch,
                  (volatile void *)LL_USART_DMA_GetRegAddr(
                      config->instance, LL_USART_DMA_REG_DATA_RECEIVE),
                  u->rx_curr_base_buf, u->rx_half_buflen);
}

// TC callback from the RULOS DMA core. Runs in DMA IRQ context. The
// user_data pointer encodes the RULOS uart_id.
static void hal_uart_on_rx_dma_tc(void *user_data) {
  uint8_t uart_id = (uint8_t)(uintptr_t)user_data;
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
  if (!u->initted) {
    return;
  }
  u->tot_ints++;
  u->tot_rx_bytes += u->rx_half_buflen;
  on_rx_buffer_full(uart_id, u, config);
}

static size_t get_rx_stored_chars_dma(uint8_t uart_id, stm32_uart_t *u,
                                      const stm32_uart_config_t *c) {
  (void)c;
  rulos_dma_stop(u->rx_dma_ch);
  const size_t free_space = rulos_dma_get_remaining(u->rx_dma_ch);
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
   * RX is initialized lazily: TX-only users of a UART never pay the
   * cost of an RX DMA channel or an RX GPIO setup. This function is
   * the single entry point that flips a UART into receive mode --
   * called once on first use, idempotent on subsequent calls.
   *
   * Note: we use the LL libraries (not the HAL) for the RX interrupt
   * path. The HAL libraries don't fit the "receive forever, one
   * character at a time" pattern RULOS wants -- they want a total
   * length up front and shut down when it's reached.
   */
  assert(uart_id < NUM_UARTS);
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  const stm32_uart_config_t *config = &stm32_uart_config[uart_id];
  if (!u->rx_gpio_initted) {
    config_gpio(config, true);
    u->rx_gpio_initted = true;
  }

#if USE_RX_DMA_FOR_CHIP
  // Lazily allocate the RX DMA channel on first start_rx. Callers that
  // only use TX never call this function and so never allocate an RX
  // channel. Subsequent calls to hal_uart_start_rx (e.g. to swap the
  // callback or buffer) reuse the existing channel.
  if (u->rx_dma_ch == NULL) {
    const rulos_dma_config_t rx_cfg = {
        .request = config->rx_dma_req,
        .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
        .mode = RULOS_DMA_MODE_NORMAL,
        .periph_width = RULOS_DMA_WIDTH_BYTE,
        .mem_width = RULOS_DMA_WIDTH_BYTE,
        .periph_increment = false,
        .mem_increment = true,
        .priority = RULOS_DMA_PRIORITY_MEDIUM,
        .tc_callback = hal_uart_on_rx_dma_tc,
        .user_data = (void *)(uintptr_t)uart_id,
    };
    u->rx_dma_ch = rulos_dma_alloc(&rx_cfg);
    if (u->rx_dma_ch == NULL) {
      __builtin_trap();
    }
  }
#endif

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

  // if this uart is expecting to rx, and a character arrived, add it to the rx
  // buffer
  if (u->rx_buf) {
      int num_read = 0;
      while (u->rx_buf && LL_USART_IsActiveFlag_RXNE(c->instance)) {
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

static void maybe_launch_next_tx(stm32_uart_t *uart) {
  assert(uart->next_sendbuf_cb != NULL);

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
    rulos_dma_start(uart->tx_dma_ch, uart->usart_tdr, (void *)buf, len);
    uart->tot_tx_bytes += len;
    if (len > uart->max_chars_per_tx_batch) {
      uart->max_chars_per_tx_batch = len;
    }
  }
}

// TC callback from the RULOS DMA core. Runs in DMA IRQ context. The
// user_data pointer encodes the RULOS uart_id.
static void hal_uart_on_tx_dma_tc(void *user_data) {
  uint8_t uart_id = (uint8_t)(uintptr_t)user_data;
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  if (!u->initted) {
    return;
  }
  // Re-arm if there's more data queued upstream.
  if (u->next_sendbuf_cb != NULL) {
    maybe_launch_next_tx(u);
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

  // Configure USART via the LL layer.
  LL_USART_Disable(config->instance);
  LL_USART_InitTypeDef usart_init = {
#if defined(RULOS_ARM_stm32c0) || defined(RULOS_ARM_stm32g0) || \
    defined(RULOS_ARM_stm32g4)
      // PrescalerValue is a modern-era field. F0/F1/F3 predate the
      // USART prescaler and don't have it in LL_USART_InitTypeDef.
      .PrescalerValue = LL_USART_PRESCALER_DIV1,
#endif
      .BaudRate = baud,
      .DataWidth = LL_USART_DATAWIDTH_8B,
      .StopBits = LL_USART_STOPBITS_1,
      .Parity = LL_USART_PARITY_NONE,
      .TransferDirection = LL_USART_DIRECTION_TX_RX,
      .HardwareFlowControl = LL_USART_HWCONTROL_NONE,
      .OverSampling = LL_USART_OVERSAMPLING_16,
  };
  if (LL_USART_Init(config->instance, &usart_init) != SUCCESS) {
    __builtin_trap();
  }
  LL_USART_Enable(config->instance);

  // Cache the TDR address so the TX DMA path can reach it without a
  // config struct lookup on every transmit. F1 is the odd one out:
  // its USART has a single DR register for both TX and RX, so
  // LL_USART_DMA_GetRegAddr() takes no direction argument.
#if defined(RULOS_ARM_stm32f1)
  uart->usart_tdr =
      (volatile void *)LL_USART_DMA_GetRegAddr(config->instance);
#else
  uart->usart_tdr = (volatile void *)LL_USART_DMA_GetRegAddr(
      config->instance, LL_USART_DMA_REG_DATA_TRANSMIT);
#endif

  // Allocate a RULOS DMA channel for TX. The core layer handles
  // channel selection, DMAMUX programming, and IRQ wiring.
  const rulos_dma_config_t tx_cfg = {
      .request = config->tx_dma_req,
      .direction = RULOS_DMA_DIR_MEM_TO_PERIPH,
      .mode = RULOS_DMA_MODE_NORMAL,
      .periph_width = RULOS_DMA_WIDTH_BYTE,
      .mem_width = RULOS_DMA_WIDTH_BYTE,
      .periph_increment = false,
      .mem_increment = true,
      .priority = RULOS_DMA_PRIORITY_LOW,
      .tc_callback = hal_uart_on_tx_dma_tc,
      .user_data = (void *)(uintptr_t)uart_id,
  };
  uart->tx_dma_ch = rulos_dma_alloc(&tx_cfg);
  if (uart->tx_dma_ch == NULL) {
    __builtin_trap();
  }

  // RX DMA channel allocation (for families that use RX DMA) is
  // deferred to hal_uart_start_rx so that TX-only users don't consume
  // a DMA channel they'll never need. Small-pool families (G031, F1
  // high-density) have only 5-7 DMA channels to share across every
  // peripheral, so saving one per TX-only UART is material.

  // Wire the UART's TX-empty signal to the DMA request line. Set once
  // at init and left on for the life of the UART; the DMA layer
  // controls whether a transfer is active via channel enable.
  LL_USART_EnableDMAReq_TX(config->instance);

  // Set up USART peripheral interrupt
  HAL_NVIC_SetPriority(config->instance_irqn, 1, 0);
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
