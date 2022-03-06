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

#include <stddef.h>
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

void rulos_hal_init();

// block interrupts/signals; returns previous state of interrupts
rulos_irq_state_t hal_start_atomic();

// restore previous interrupt state
void hal_end_atomic(rulos_irq_state_t old_interrupts);

void hal_deep_sleep();
void hal_idle();  // hw: spin. sim: sleep

#define HAL_MAGIC 0x74

///// timer/clock HAL

#define TIMER0 (0)
#define TIMER1 (1)
#define TIMER2 (2)

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data,
                            uint8_t timer_id);
bool hal_clock_interrupt_is_pending();
// how far is the clock into its current tick, out of 10,000?
uint16_t hal_elapsed_tenthou_intervals();
void hal_speedup_clock_ppm(int32_t ratio);
void hal_delay_ms(uint16_t ms);

//// 7-segment panel HAL

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment,
                         uint8_t onoff);
void hal_7seg_bus_enter_sleep();  // Call to stop driving 7seg bus

////
char hal_read_keybuf();
char hal_scan_keypad();

void hal_init_keypad();
void hal_init_adc(Time scan_period);
void hal_init_adc_channel(uint8_t idx);
uint16_t hal_read_adc(uint8_t idx);

void hal_init_joystick_button();
bool hal_read_joystick_button();

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
