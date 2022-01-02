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

#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "rom/gpio.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/uart_struct.h"

typedef struct {
  uart_dev_t *dev;  // pointer to actual hardware registers
  uint8_t uart_id;
  hal_uart_next_sendbuf_cb tx_cb;
  hal_uart_receive_cb rx_cb;
  void *cb_data;
} esp32_uart_t;

#define NUM_UARTS   3
#define UART_BUFLEN 128  // defined by hardware; see datasheet

static esp32_uart_t esp32_uart[NUM_UARTS] = {
    {
        .dev = &UART0,
        .uart_id = 0,
    },
    {
        .dev = &UART1,
        .uart_id = 1,
    },
    {
        .dev = &UART2,
        .uart_id = 2,
    },
};

void hal_uart_init(uint8_t uart_id, uint32_t baud,
                   void *user_data /* for both rx and tx upcalls */,
                   uint16_t *max_tx_len /* OUT */) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];

  // save RULOS callback data and tell uart layer the max size of writes we're
  // willing to acccept
  eu->cb_data = user_data;
  *max_tx_len = 100;  // hardware buffer is 128 bytes; we configure the
                      // interrupt to fire when buffer fullness get below 27

  // configure the underlying hardware
  int rx_pin, rx_sig;
  int tx_pin, tx_sig;
  switch (uart_id) {
    case 0:
      DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART_CLK_EN);
      DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART_RST);
      rx_pin = 3;
      rx_sig = U0RXD_IN_IDX;
      tx_pin = 1;
      tx_sig = U0TXD_OUT_IDX;
      break;
    case 1:
      DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART1_CLK_EN);
      DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART1_RST);
      rx_pin = 9;
      rx_sig = U1RXD_IN_IDX;
      tx_pin = 10;
      tx_sig = U1TXD_OUT_IDX;
      break;
    case 2:
      DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
      DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);
      rx_pin = 16;
      rx_sig = U2RXD_IN_IDX;
      tx_pin = 17;
      tx_sig = U2TXD_OUT_IDX;
      break;
  }

  const bool inverted = false;
  gpio_make_input_disable_pullup(rx_pin);
  gpio_matrix_in(rx_pin, rx_sig, inverted);
  gpio_make_output(tx_pin);
  gpio_matrix_in(tx_pin, tx_sig, inverted);

  eu->dev->conf0.val = 0;
  eu->dev->conf0.tick_ref_always_on =
      1;  // cargo-culted from esp32-hal-uart.h // SERIAL_8N1
  eu->dev->conf0.bit_num = 3;       // 8 data bits
  eu->dev->conf0.parity_en = 0;     // no parity
  eu->dev->conf0.stop_bit_num = 1;  // 1 stop bit

  // baud-rate config also copied from esp32-hal-uart.c
  uint32_t clk_div = ((getApbFrequency() << 4) / baud);
  eu->dev->clk_div.div_int = clk_div >> 4;
  eu->dev->clk_div.div_frag = clk_div & 0xf;

  eu->dev->idle_conf.tx_idle_num = 0;  // cargo-cult
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb) {
  assert(false);  // not implemented yet
}

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];

  while (true) {
    uint16_t this_send_len;
    const char *this_buf;
    cb(uart_id, eu->cb_data, &this_buf, &this_send_len);
    if (this_send_len == 0) {
      return;
    }
    while (this_send_len > 0) {
      while (eu->dev->status.txfifo_cnt == 0x7F)
        ;
      eu->dev->fifo.rw_byte = *this_buf++;
      this_send_len--;
    }
  }
}
