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

#include <stdint.h>

#include "core/hardware_types.h"
#include "core/heap.h"
#include "core/media.h"
#include "core/util.h"

/*
 * Functions that are separately implemented by both the simulator and the real
 * hardware
 */

typedef void (*Handler)(void *data);

void hal_init();

// block interrupts/signals; returns previous state of interrupts
rulos_irq_state_t hal_start_atomic();

// restore previous interrupt state
void hal_end_atomic(rulos_irq_state_t old_interrupts);

void hal_deep_sleep();
void hal_idle();  // hw: spin. sim: sleep

#define HAL_MAGIC 0x74

#define TIMER0 (0)
#define TIMER1 (1)
#define TIMER2 (2)

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data,
                            uint8_t timer_id);

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment,
                         uint8_t onoff);
void hal_7seg_bus_enter_sleep();  // Call to stop driving 7seg bus
char hal_read_keybuf();
char hal_scan_keypad();
// used for "hold a key at startup" check.
uint16_t hal_elapsed_milliintervals();
void hal_speedup_clock_ppm(int32_t ratio);
void hal_delay_ms(uint16_t ms);

void hal_init_keypad();
void hal_init_adc(Time scan_period);
void hal_init_adc_channel(uint8_t idx);
uint16_t hal_read_adc(uint8_t idx);

void hal_init_joystick_button();
bool hal_read_joystick_button();

/////////////// UART ///////////////////////////////////////////////

// At the HAL layer, UARTs are identified by integers. hal_uart_init initializes
// a UART. max_tx_len is an out parameter that describes the maximum length of a
// tx the UART can accept at a time.
void hal_uart_init(uint8_t uart_id, uint32_t baud, bool stop2,
                   void *user_data /* for both rx and tx upcalls */,
                   uint16_t *max_tx_len /* OUT */);

// Callback for incoming serial data. If a callback is set using
// hal_uart_set_receive_cb, incoming characters will be passed into that
// callback at interrupt time.
typedef void (*hal_uart_receive_cb)(uint8_t uart_id, void *user_data, char c);

// Enable reception on this UART; when a char is received, the provided upcall
// is called at interrupt time.
void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb);

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

/////////////// TWI ///////////////////////////////////////////////

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *trs);

////////////////// Audio ////////////////////////////////////////////

typedef struct s_HALSPIHandler {
  void (*func)(struct s_HALSPIHandler *handler, uint8_t data);
} HALSPIHandler;

#define SPIFLASH_CMD_READ_DATA (0x03)

void hal_audio_init(void);
void hal_audio_fire_latch(void);
void hal_audio_shift_sample(uint8_t sample);
