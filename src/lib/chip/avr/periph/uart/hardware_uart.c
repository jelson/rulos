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
 * hardware_uart.c: UART
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <string.h>
#include <util/delay_basic.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

void audioled_set(r_bool red, r_bool yellow);

uint16_t baud_to_ubrr(uint32_t baud) {
  return ((uint32_t)hardware_f_cpu) / 16 / baud - 1;
}

typedef struct {
  hal_uart_receive_cb rx_cb;
  hal_uart_next_sendbuf_cb next_sendbuf_cb;
  void *user_data;
} avr_uart_state_t;

avr_uart_state_t g_uart_state[2] = {};

#if defined(MCU8_line)
// Only one UART, named without an index

#define _UBRRH UBRRH
#define _UBRRL UBRRL
#define _UCSRA UCSRA
#define _UCSRB UCSRB
#define _UCSRC UCSRC
#define _RXEN RXEN
#define _RXCIE RXCIE
#define _TXEN TXEN
#define _TXCIE TXCIE
#define _UCSZ0 UCSZ0
#define _UCSZ1 UCSZ1
#define _UDR UDR
#define _UDRE UDRE
#define _UDRIE UDRIE
#define _USBS USBS
#define _USART_RXC_vect USART_RXC_vect
#define _USART_UDRE_vect USART_UDRE_vect
#define hal_uart_init_name hal_uart_init0
#define enable_sendready_interrupt_name enable_sendready_interrupt0
#define hal_uart_start_rx_name hal_uart_start_rx0
#define hal_uart_start_send_name hal_uart_start_send0
#define _handle_recv_ready_name _handle_recv_ready0
#define _handle_send_ready_name _handle_send_ready0
#define sync_send_byte_name sync_send_byte0
#define hal_uart_sync_send_bytes_name hal_uart_sync_send_bytes0
#define hal_uart_sync_send_name hal_uart_sync_send0
#define UARTID 0
#define HAVE_UARTID0 1
#include "hardware_uart.ch"

#endif

#if defined(MCU328_line) || defined(MCU1284_line)
// One or two uarts; first uart has 0 in its name.
// Except the vector names. Geez what a mess.

#if defined(MCU328_line)
#define _USART_RXC_vect USART_RX_vect
#define _USART_UDRE_vect USART_UDRE_vect
#elif defined(MCU1284_line)
#define _USART_RXC_vect USART0_RX_vect
#define _USART_UDRE_vect USART0_UDRE_vect
#else
#error missing arch
#endif

#define _UBRRH UBRR0H
#define _UBRRL UBRR0L
#define _UCSRA UCSR0A
#define _UCSRB UCSR0B
#define _UCSRC UCSR0C
#define _RXEN RXEN0
#define _RXCIE RXCIE0
#define _TXEN TXEN0
#define _TXCIE TXCIE0
#define _UCSZ0 UCSZ00
#define _UCSZ1 UCSZ01
#define _UDR UDR0
#define _UDRE UDRE0
#define _UDRIE UDRIE0
#define _USBS USBS0
#define hal_uart_init_name hal_uart_init0
#define enable_sendready_interrupt_name enable_sendready_interrupt0
#define hal_uart_start_rx_name hal_uart_start_rx0
#define hal_uart_start_send_name hal_uart_start_send0
#define _handle_recv_ready_name _handle_recv_ready0
#define _handle_send_ready_name _handle_send_ready0
#define sync_send_byte_name sync_send_byte0
#define hal_uart_sync_send_bytes_name hal_uart_sync_send_bytes0
#define hal_uart_sync_send_name hal_uart_sync_send0
#define UARTID 0
#define HAVE_UARTID0 1
#include "hardware_uart.ch"

#undef _UBRRH
#undef _UBRRL
#undef _UCSRA
#undef _UCSRB
#undef _UCSRC
#undef _RXEN
#undef _RXCIE
#undef _TXEN
#undef _TXCIE
#undef _UCSZ0
#undef _UCSZ1
#undef _UDR
#undef _UDRE
#undef _UDRIE
#undef _USBS
#undef _USART_RXC_vect
#undef _USART_UDRE_vect
#undef hal_uart_init_name
#undef enable_sendready_interrupt_name
#undef hal_uart_start_rx_name
#undef hal_uart_start_send_name
#undef _handle_recv_ready_name
#undef _handle_send_ready_name
#undef sync_send_byte_name
#undef hal_uart_sync_send_name
#undef hal_uart_sync_send_bytes_name
#undef UARTID

#endif

#if defined(MCU1284_line)
// Two uarts; second uart has 1 in its name.

#define _UBRRH UBRR1H
#define _UBRRL UBRR1L
#define _UCSRA UCSR1A
#define _UCSRB UCSR1B
#define _UCSRC UCSR1C
#define _RXEN RXEN1
#define _RXCIE RXCIE1
#define _TXEN TXEN1
#define _TXCIE TXCIE1
#define _UCSZ0 UCSZ10
#define _UCSZ1 UCSZ11
#define _UDR UDR1
#define _UDRE UDRE1
#define _UDRIE UDRIE1
#define _USBS USBS1
#define _USART_RXC_vect USART1_RX_vect
#define _USART_UDRE_vect USART1_UDRE_vect
#define hal_uart_init_name hal_uart_init1
#define enable_sendready_interrupt_name enable_sendready_interrupt1
#define hal_uart_start_rx_name hal_uart_start_rx1
#define hal_uart_start_send_name hal_uart_start_send1
#define _handle_recv_ready_name _handle_recv_ready1
#define _handle_send_ready_name _handle_send_ready1
#define sync_send_byte_name sync_send_byte1
#define hal_uart_sync_send_bytes_name hal_uart_sync_send_bytes1
#define hal_uart_sync_send_name hal_uart_sync_send1
#define UARTID 1
#define HAVE_UARTID1 1
#include "hardware_uart.ch"

#endif

void hal_uart_init(uint8_t uart_id, uint32_t baud, r_bool stop2,
                   void *user_data, uint16_t *max_tx_len) {
  *max_tx_len = 1;
  switch (uart_id) {
#if HAVE_UARTID0
  case 0:
    hal_uart_init0(baud, stop2, user_data);
    return;
#endif
#if HAVE_UARTID1
  case 1:
    hal_uart_init1(baud, stop2, user_data);
    return;
#endif
  default:
    assert(false);
  }
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb) {
  switch (uart_id) {
#if HAVE_UARTID0
  case 0:
    hal_uart_start_rx0(rx_cb);
    return;
#endif
#if HAVE_UARTID1
  case 1:
    hal_uart_start_rx1(rx_cb);
    return;
#endif
  default:
    assert(false);
  }
}

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  switch (uart_id) {
#if HAVE_UARTID0
  case 0:
    hal_uart_start_send0(cb);
    return;
#endif
#if HAVE_UARTID1
  case 1:
    hal_uart_start_send1(cb);
    return;
#endif
  default:
    assert(false);
  }
}
