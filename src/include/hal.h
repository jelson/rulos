#ifndef __hal_h__
#define __hal_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)();

void hal_init();
void hal_start_atomic();	// block interrupts/signals
void hal_end_atomic();		// resume interrupts/signals
void hal_idle();			// hw: spin. sim: sleep
void hal_start_clock_us(uint32_t us, Handler handler);
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff);
char hal_read_keybuf();
uint16_t hal_elapsed_milliintervals();
void hal_uart_init(uint16_t baud);

#endif // __hal_h__
