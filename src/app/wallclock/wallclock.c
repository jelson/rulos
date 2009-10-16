#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "uart.h"


#define MILLION 1000000

typedef struct {
	ActivationFunc f;
	BoardBuffer bbuf;
	Time last_redraw_time;
	int hour;
	int minute;
	int second;
	int hundredth;

	int unhappy_state;
	int unhappy_timer;

	// uart time-setting stuff
	UartQueue_t *uq;
	Time last_reception_us;
} WallClockActivation_t;



/******************** Display *******************************/

static void display_clock(WallClockActivation_t *wca)
{
	char buf[9];

	int_to_string2(buf,   2, 1, wca->hour);
	int_to_string2(buf+2, 2, 2, wca->minute);
	int_to_string2(buf+4, 2, 2, wca->second);
	int_to_string2(buf+6, 2, 2, wca->hundredth);
	ascii_to_bitmap_str(wca->bbuf.buffer, 8, buf);
	wca->bbuf.buffer[1] |= SSB_DECIMAL;
	wca->bbuf.buffer[2] |= SSB_DECIMAL;
	wca->bbuf.buffer[3] |= SSB_DECIMAL;
	wca->bbuf.buffer[4] |= SSB_DECIMAL;
	wca->bbuf.buffer[5] |= SSB_DECIMAL;
}


static void display_unhappy(WallClockActivation_t *wca, uint16_t interval_ms)
{
	wca->unhappy_timer += interval_ms;

	LOGF((logfp, "unhappy timer: %d\n", wca->unhappy_timer));

	if (wca->unhappy_timer > 1500)
	{
		wca->unhappy_state++;
		wca->unhappy_timer = 0;
	}
	if (wca->unhappy_state >= 2)
		wca->unhappy_state = 0;
	
	if (wca->unhappy_state == 0)
		ascii_to_bitmap_str(wca->bbuf.buffer, 8, "NEEd");
	else if (wca->unhappy_state == 1)
		ascii_to_bitmap_str(wca->bbuf.buffer, 8, " PC ");
}



/****************  Clock Management *****************************/

static void calibrate_clock(WallClockActivation_t *wca,
							uint8_t hour,
							uint8_t minute,
							uint8_t second,
							Time reception_us)
{
	// Compute the elapsed time according to our local clock between
	// this packet received and when the previous one was received
	Time reception_diff_us = reception_us - wca->last_reception_us;

	// Update the wall-clock and record when we received this pulse
	wca->hour = hour;
	wca->minute = minute;
	wca->second = second;
	wca->hundredth = 0;
	wca->last_reception_us = reception_us;
    
	// There were supposed to be 60 seconds between this one and the last.
	// TODO: Support arbitrary intervals.
	Time error = 60*MILLION - reception_diff_us;
	
	// If it's off by 20 seconds or more, don't use this as a
	// calibration.
	if (error > 20*MILLION || error < -20*MILLION)
		return;

	// Compute the ratio by which we're off in parts per million and
	// update the clock rate
	Time ratio = error / 60;
	hal_speedup_clock_ppm(ratio);
}


/*
 * Advance the display clock by a given number of milliseconds
 */
static void advance_clock(WallClockActivation_t *wca, uint16_t interval_ms)
{
	// if we don't have a valid time, don't change the clock
	if (wca->hour < 0)
		return;

	wca->hundredth += (interval_ms / 10);

	while (wca->hundredth >= 100) {
		wca->hundredth -= 100;
		wca->second++;
	}
	while (wca->second >= 60) {
		wca->second -= 60;
		wca->minute++;
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
	uint8_t msg[8];

	hal_start_atomic();

	// In case of framing error, clear the queue
	if (ByteQueue_peek(wca->uq->q, &msg[0]) && msg[0] != 'T') {
		uart_queue_reset();
		goto done;
	}

	// If there are fewer than 8 characters, it could just be that the
	// message is still in transit; do nothing.
	if (ByteQueue_length(wca->uq->q) < 8)
		goto done;

	// There are (at least) 8 characters - great!  Copy them in and
	// reset the queue.
	ByteQueue_pop_n(wca->uq->q, msg, 8);
	uart_queue_reset();

	// Make sure both framing characters are correct
	if (msg[0] != 'T' || msg[7] != 'E')
		goto done;

	// Decode the characters
	uint8_t hour   = ascii_digit(msg[1])*10 + ascii_digit(msg[2]);
	uint8_t minute = ascii_digit(msg[3])*10 + ascii_digit(msg[4]);
	uint8_t second = ascii_digit(msg[5])*10 + ascii_digit(msg[6]);

	// Update the clock
	calibrate_clock(wca, hour, minute, second, wca->uq->reception_time_us);

 done:
	hal_end_atomic();

     
}

static void update(WallClockActivation_t *wca)
{
	// compute how much time has passed since we were last called
	Time now = clock_time_us();
	Time interval_us = now - wca->last_redraw_time;
	uint16_t interval_ms = (interval_us + 500) / 1000;
	LOGF((logfp, "interval: %d\n", interval_ms));
	wca->last_redraw_time = now;

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
	board_buffer_draw(&wca->bbuf);


	// schedule the next callback
	schedule_us(10000, (Activation *) wca);
}


int main()
{
	heap_init();
	util_init();
	hal_init();
	board_buffer_module_init();

	// start clock with 10 msec resolution
	clock_init(10000);

	// start the uart running
	uart_init(3);

	// initialize our internal state
	WallClockActivation_t wca;
	memset(&wca, sizeof(wca), 0);
	wca.f = (ActivationFunc) update;
	wca.hour = -1;
	wca.unhappy_timer = 0;
	wca.uq = uart_queue_get();

	// init the board buffer
	board_buffer_init(&wca.bbuf);
	wca.bbuf.upside_down = (1 << 2) | (1 << 4);
	board_buffer_push(&wca.bbuf, 0);

	// have the callback get called immediately
	schedule_us(1, (Activation *) &wca);

	cpumon_main_loop();
	return 0;
}
