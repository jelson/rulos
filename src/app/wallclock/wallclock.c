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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/rulos.h"
#include "core/queue.h"
#include "periph/uart/uart.h"
#include "periph/7seg_panel/board_buffer.h"

#ifdef SIM
# include "sim.h"
#endif


#define MILLION 1000000
#define WALLCLOCK_CALLBACK_INTERVAL 10000

typedef struct {
	BoardBuffer bbuf;
	Time last_redraw_time;
	int hour;
	int minute;
	int second;
	int hundredth;

	int unhappy_state;
	int unhappy_timer;

	UartQueue_t *recvQueue;
	Time last_reception_us;
	int mins_since_last_sync;

	uint8_t display_flag;
} WallClockActivation_t;



/******************** Display *******************************/

int precision_threshold_times_min[] = {
	1,   // lose hundredths of a sec
	3,  // lose tenths of a sec
	20, // lose seconds
	1000  // demand resync
};

static void display_clock(WallClockActivation_t *wca)
{
	char buf[9];

	// draw the base numbers
	int_to_string2(buf,   2, 1, wca->hour);
	int_to_string2(buf+2, 2, 2, wca->minute);
	int_to_string2(buf+4, 2, 2, wca->second);
	int_to_string2(buf+6, 2, 2, wca->hundredth);

	// delete some if we haven't resynced the clock in a while
	if (wca->mins_since_last_sync > precision_threshold_times_min[0]) {
		buf[7] = ' ';
	}
	if (wca->mins_since_last_sync > precision_threshold_times_min[1]) {
		buf[6] = ' ';
	}
	if (wca->mins_since_last_sync > precision_threshold_times_min[2]) {
		buf[5] = ' ';
		buf[4] = ' ';
	}
	if (wca->mins_since_last_sync > precision_threshold_times_min[3]) {
		wca->hour = -1;
	}

	ascii_to_bitmap_str(wca->bbuf.buffer, 8, buf);
	wca->bbuf.buffer[1] |= SSB_DECIMAL;
	wca->bbuf.buffer[2] |= SSB_DECIMAL;
	wca->bbuf.buffer[3] |= SSB_DECIMAL;
	wca->bbuf.buffer[4] |= SSB_DECIMAL;
	wca->bbuf.buffer[5] |= SSB_DECIMAL;
}


static void display_unhappy(WallClockActivation_t *wca, uint16_t interval_ms)
{
	const char *msg[] = { " NEEd ", "  PC  ", "SErIAL", "      ", NULL };

	wca->unhappy_timer += interval_ms;

	if (wca->unhappy_timer > 1500)
	{
		wca->unhappy_state++;
		wca->unhappy_timer = 0;
	}
	if (msg[wca->unhappy_state] == NULL)
		wca->unhappy_state = 0;

	ascii_to_bitmap_str(wca->bbuf.buffer, 8, msg[wca->unhappy_state]);
}



/****************  Clock Management *****************************/

#define SECONDS_BETWEEN_PULSES 60

static void calibrate_clock(WallClockActivation_t *wca,
			    uint8_t hour,
			    uint8_t minute,
			    uint8_t second,
			    Time reception_us)
{
	// Compute the elapsed time according to our local clock between
	// this packet received and when the previous one was received
	Time reception_diff_us, error;

	if (wca->last_reception_us && reception_us > wca->last_reception_us) {
		reception_diff_us = reception_us - wca->last_reception_us;
		// There were supposed to be 60 seconds between this one and
		// the last.  TODO: Support arbitrary intervals.
		error = SECONDS_BETWEEN_PULSES*MILLION - reception_diff_us;
	}
	else {
		reception_diff_us = -1;
		error = 0;
	}

	LOG("got pulse at %d, last at %d, diff is %d, error %d\n", 
		  reception_us, wca->last_reception_us, reception_diff_us, error);
	wca->last_reception_us = reception_us;
	wca->mins_since_last_sync = 0;


	// Update the wall-clock.  Account for the time between when we
	// received the pulse and the current time.
	Time time_past_pulse = precise_clock_time_us() - reception_us;
	uint8_t extra_seconds = time_past_pulse / 1000000;
	uint8_t extra_hundredths = (time_past_pulse % 1000000) / 10000;
	wca->hour = hour;
	wca->minute = minute;
	wca->second = second + extra_seconds;
	wca->hundredth = extra_hundredths;

	// If it's off by 5 seconds or more, don't use this as a
	// calibration.
	if (error > 5*MILLION || error < -5*MILLION) {
		LOG("error too large -- not correcting\n");
		return;
	}

	// Compute the ratio by which we're off in parts per million and
	// update the clock rate.  Then divide by 2, to give us an
	// exponentially weighted moving average of the clock rate.
	if (error) {
		Time ratio = error / SECONDS_BETWEEN_PULSES / 2;
		hal_speedup_clock_ppm(ratio);
	}
}


/*
 * Advance the display clock by a given number of milliseconds
 */
static void advance_clock(WallClockActivation_t *wca, uint16_t interval_ms)
{
	// if we don't have a valid time, don't change the clock
	if (wca->hour < 0)
		return;

	wca->hundredth += ((interval_ms + 5) / 10);

	while (wca->hundredth >= 100) {
		wca->hundredth -= 100;
		wca->second++;
	}
	while (wca->second >= 60) {
		wca->second -= 60;
		wca->minute++;
		wca->mins_since_last_sync++;
	}
	while (wca->minute >= 60) {
		wca->minute -= 60;
		wca->hour++;
	}
	while (wca->hour >= 24) {
		wca->hour -= 24;
	}
}


/******************* main ***********************************/

static uint8_t ascii_digit(uint8_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else
		return 0;
}

// Check the UART to see if there's a valid message.  I'll arbitrarily
// define a protocol: messages are 8 ASCII characters: T123055E
// ("Time 12:30:55 End").  So, we'll check the uart periodically.
// If the buffer doesn't begin with T, we assume a framing error and
// throw the whole buffer away.
//
// Note that the UART timestamping code returns a timestamp only for
// the first character in the queue.

static void check_uart(WallClockActivation_t *wca)
{
	char msg[8];
	uint8_t hour, minute, second;
	Time reception_time_us;

	uint8_t old_flags = hal_start_atomic();

	if (CharQueue_length(wca->recvQueue->q) == 0)
		goto done;

	// In case of framing error, clear the queue
	if (CharQueue_peek(wca->recvQueue->q, &msg[0]) && msg[0] != 'T') {
		LOG("first char mismatch\n");
		uart_reset_recvq(wca->recvQueue);
		goto done;
	}

	// If there are fewer than 8 characters, it could just be that the
	// message is still in transit; do nothing.
	if (CharQueue_length(wca->recvQueue->q) < 8) {
		goto done;
	}

	// There are (at least) 8 characters - great!  Copy them in and
	// reset the queue.
	CharQueue_pop_n(wca->recvQueue->q, msg, 8);
	reception_time_us = wca->recvQueue->reception_time_us;
	uart_reset_recvq(wca->recvQueue);

	// Make sure both framing characters are correct
	if (msg[0] != 'T' || msg[7] != 'E')
		goto done;

	// Decode the characters
	hour   = ascii_digit(msg[1])*10 + ascii_digit(msg[2]);
	minute = ascii_digit(msg[3])*10 + ascii_digit(msg[4]);
	second = ascii_digit(msg[5])*10 + ascii_digit(msg[6]);

	// Update the clock
	calibrate_clock(wca, hour, minute, second, reception_time_us);

	// indicate we got a message
	wca->display_flag = 25;

 done:
	hal_end_atomic(old_flags);
}


static void update(WallClockActivation_t *wca)
{
	// compute how much time has passed since we were last called
	Time now = clock_time_us();
	Time interval_us = now - wca->last_redraw_time;
	wca->last_redraw_time = now;
	uint16_t interval_ms = (interval_us + 500) / 1000;


	// advance the clock by that amount
	advance_clock(wca, interval_ms);

	// check and see if we've gotten any uart messages
	check_uart(wca);

	// display either the time (if the clock has been set)
	// or an unhappy message (if it has not)
	if (wca->hour < 0)
		display_unhappy(wca, interval_ms);
	else
		display_clock(wca);

	if (wca->display_flag) {
		wca->display_flag--;
		wca->bbuf.buffer[7] |= SSB_DECIMAL;
	}

	board_buffer_draw(&wca->bbuf);

	// schedule the next callback
	schedule_us(WALLCLOCK_CALLBACK_INTERVAL, (ActivationFuncPtr) update, wca);
}


int main()
{
	hal_init();
	hal_init_rocketpanel(bc_wallclock);
	board_buffer_module_init();

	// start clock with 10 msec resolution
	init_clock(WALLCLOCK_CALLBACK_INTERVAL, TIMER1);

	// start the uart running at 34k baud
	UartState_t uart;
	uart_init(&uart, 38400, TRUE, 0);

	// initialize our internal state
	WallClockActivation_t wca;
	memset(&wca, 0, sizeof(wca));
	wca.hour = -1;
	wca.unhappy_timer = 0;
	wca.unhappy_state = 0;
	wca.last_redraw_time = clock_time_us();
	wca.mins_since_last_sync = 0;
	wca.recvQueue = uart_recvq(&uart);

	// init the board buffer
	board_buffer_init(&wca.bbuf);
	board_buffer_push(&wca.bbuf, 0);

	// have the callback get called immediately
	schedule_us(1, (ActivationFuncPtr) update, &wca);

	cpumon_main_loop();
	return 0;
}
