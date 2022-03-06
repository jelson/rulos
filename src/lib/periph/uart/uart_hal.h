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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// At the HAL layer, UARTs are identified by integers. hal_uart_init initializes
// a UART. max_tx_len is an out parameter that describes the maximum length of a
// tx the UART can accept at a time.
void hal_uart_init(uint8_t uart_id, uint32_t baud,
                   void *user_data /* for both rx and tx upcalls */,
                   size_t *max_tx_len /* OUT */);

// Callback for incoming serial data. If a callback is set using
// hal_uart_set_receive_cb, incoming characters will be passed into that
// callback at interrupt time.
typedef void (*hal_uart_receive_cb)(uint8_t uart_id, void *user_data, char *buf,
                                    size_t len);

// Enable reception on this UART. Buffer and its capacity must be provided.
// Buffer must be even length since it's divided into two halves. The driver
// will fill half the buffer while providing an upcall to the given callback on
// the other half. Warning: upcalls *might* be at interrupt time, but will not
// be if called in response to hal_uart_trigger_rx();
void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb, void *buf,
                       size_t buflen);

// Downcall to indicate the top layer has finished processing the last data
// passed up by rx_cb, and it's ready to receive the next.
void hal_uart_rx_cb_done(uint8_t uart_id);

// Begin a train of transmissions to a uart. When each completes, the send_next
// callback will be called at interrupt time to retrieve the next data to be
// sent.
//
// The value returned as the max_tx_len out param of hal_uart_init the maximum
// size of a buffer that can be returned on each callback. This is to deal with
// the hardware differences between AVR-class devices, which can only transmit
// one character per interrupt, and ARM-class devices that support DMA.
//
// If there is no more data to send, len should be set to 0.
typedef void (*hal_uart_next_sendbuf_cb)(uint8_t uart_id, void *user_data,
                                         const char **tx_buf /*OUT*/,
                                         uint16_t *len /*OUT*/);
void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb);

// write stats to the log
void hal_uart_log_stats(uint8_t uart_id);
