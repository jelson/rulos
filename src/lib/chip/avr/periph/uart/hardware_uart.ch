/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

// This file contains functions statically parameterized by UART compiler
// definitions; we include it multiple times to provide access to both
// UARTs.

void hal_uart_init_name(uint32_t baud, void *user_data) {
  uint16_t ubrr = baud_to_ubrr(baud);

  g_uart_state[UARTID].user_data = user_data;

  // disable interrupts
  cli();

  // set up baud rate
  _UBRRH = (unsigned char)(ubrr >> 8);
  _UBRRL = (unsigned char)ubrr;

  _UCSRB = _BV(_TXEN) |  // enable transmitter
           _BV(_RXEN)    // enable receiver
      ;

  // set frame format: async, 8 bit data, 1 stop bit, no parity
  _UCSRC = _BV(_UCSZ1) | _BV(_UCSZ0)
#ifdef MCU8_line
           | _BV(URSEL)
#endif
      ;

  // enable interrupts, whether or not they'd been previously enabled
  sei();
}

//// rx

void hal_uart_start_rx_name(hal_uart_receive_cb rx_cb) {
  g_uart_state[UARTID].rx_cb = rx_cb;

  // enable receiver interrupt
  _UCSRB |= _BV(_RXCIE);
}

// Runs in interrupt context.
static inline void _handle_recv_ready_name(char c) {
  if (g_uart_state[UARTID].rx_cb != NULL) {
    g_uart_state[UARTID].rx_cb(
        UARTID,
        g_uart_state[UARTID].user_data,
        c);
  }
}

//// tx

static inline void enable_sendready_interrupt_name(uint8_t enable) {
  if (enable) {
    reg_set(&_UCSRB, _UDRIE);
  } else {
    reg_clr(&_UCSRB, _UDRIE);
  }
}

// Runs in user context.
void hal_uart_start_send_name(hal_uart_next_sendbuf_cb cb) {
  // Just enable the send-ready interrupt.  It will fire as soon as
  // the sender register is free.
  g_uart_state[UARTID].next_sendbuf_cb = cb;
  uint8_t old_interrupts = hal_start_atomic();
  enable_sendready_interrupt_name(TRUE);
  hal_end_atomic(old_interrupts);
}

// Runs in interrupt context.
static inline void _handle_send_ready_name() {
  // If there is still data remaining to send, send it.  Otherwise,
  // disable the send-ready interrupt.
  assert(g_uart_state[UARTID].next_sendbuf_cb != NULL);

  const char *c;
  uint16_t tx_len;
  g_uart_state[UARTID].next_sendbuf_cb(
      UARTID,
      g_uart_state[UARTID].user_data,
      &c,
      &tx_len);
  assert(tx_len <= 1);
  if (tx_len == 0) {
    enable_sendready_interrupt_name(FALSE);
  } else {
    _UDR = *c;
  }
}

ISR(_USART_RXC_vect) { _handle_recv_ready_name(_UDR); }
ISR(_USART_UDRE_vect) { _handle_send_ready_name(); }

static inline void sync_send_byte_name(const uint8_t c) {
  while (!(_UCSRA & _BV(_UDRE))) {
    ;
  }
  _UDR = c;
}

void hal_uart_sync_send_bytes_name(const uint8_t *s, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    sync_send_byte_name(*s++);
  }
}

void hal_uart_sync_send_name(const char *s) {
  while (*s != '\0') {
    sync_send_byte_name(*s++);
  }
}
