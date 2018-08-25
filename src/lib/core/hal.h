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

#pragma once

#include "core/heap.h"
#include "core/media.h"
#include "core/util.h"
#include "hardware_types.h"

/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)(void *data);

void hal_init();

// block interrupts/signals; returns previous state of interrupts
rulos_irq_state_t hal_start_atomic();

// restore previous interrupt state
void hal_end_atomic(rulos_irq_state_t old_interrupts);

void hal_deep_sleep();
void hal_idle();			// hw: spin. sim: sleep

#define HAL_MAGIC 0x74

#define TIMER0  (0)
#define TIMER1	(1)
#define TIMER2	(2)

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id);

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff);
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
r_bool hal_read_joystick_button();

/////////////// UART ///////////////////////////////////////////////

struct s_UartHandler;
typedef void (hal_uart_receive_fp)(struct s_UartHandler *u, char c);
typedef r_bool (hal_uart_send_next_fp)(struct s_UartHandler *u, char *c /*OUT*/);
typedef struct s_UartHandler {
	hal_uart_receive_fp *recv;	// runs in interrupt context
	hal_uart_send_next_fp *send;	// runs in interrupt context
	uint8_t uart_id;
} UartHandler;

void hal_uart_init(UartHandler *handler, uint32_t baud, r_bool stop2, uint8_t uart_id);
void hal_uart_start_send(UartHandler *handler);
void hal_uart_sync_send(UartHandler *handler, char *s, uint8_t len);

/////////////// TWI ///////////////////////////////////////////////

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr, MediaRecvSlot *trs);

////////////////// Audio ////////////////////////////////////////////

typedef struct s_HALSPIHandler {
	void (*func)(struct s_HALSPIHandler *handler, uint8_t data);
} HALSPIHandler;

#define SPIFLASH_CMD_READ_DATA	(0x03)

void hal_init_spi();
void hal_spi_set_fast(r_bool fast);
void hal_spi_select_slave(r_bool select);
void hal_spi_set_handler(HALSPIHandler *handler);
void hal_spi_send(uint8_t bte);

void hal_audio_init(void);
void hal_audio_fire_latch(void);
void hal_audio_shift_sample(uint8_t sample);
