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

#ifndef __hal_h__
#define __hal_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#include "media.h"

/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)(void *data);

typedef enum {
	bc_rocket0,
	bc_rocket1,
	bc_audioboard,
	bc_wallclock,
	bc_chaseclock,
} BoardConfiguration;

void hal_init(BoardConfiguration bc);

// block interrupts/signals; returns previous state of interrupts
uint8_t hal_start_atomic(void);

// restore previous interrupt state
void hal_end_atomic(uint8_t old_interrupts);

void hal_idle();			// hw: spin. sim: sleep

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
} UartHandler;

void hal_uart_init(UartHandler *handler, uint32_t baud, r_bool stop2);
void hal_uart_start_send(void);

/////////////// TWI ///////////////////////////////////////////////

MediaStateIfc *hal_twi_init(Addr local_addr, MediaRecvSlot *trs);

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

#endif // __hal_h__
