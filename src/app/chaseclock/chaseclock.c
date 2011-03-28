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

#include "rocket.h"
#include "uart.h"
#include "sim.h"

/* Chase's definitions */

#define RAND_OFFSET_LIMIT 10
#define SET_RAND_HOUR 12		// Time at which to reset the random offset
#define SET_RAND_MIN  0
#define SET_RAND_SEC  0
//#define PRINT_DAYDATE

typedef enum {
	JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC
} Month;

typedef enum {
	SUN, MON, TUE, WED, THU, FRI, SAT
} Day;

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	r_bool  pm;
	Day day;
	uint8_t date;
	Month month;
	uint8_t randoffmin;
} ChaseTime_t;

ChaseTime_t initTime = {
	11, 59, 50, TRUE, SAT, 31, DEC, 0		/* Set initial time to 11:59.50 pm, Sat Dec 31, no offset */
};

char daystrings[7][3] = {
	"SU", "MO", "TU", "WE", "TH", "FR", "SA"
};

uint8_t days_in_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
/* End Chase's definitions */


typedef struct {
	ActivationFunc func;
	Time last_run;
	BoardBuffer bbuf;
	uint32_t secs;
	ChaseTime_t time;
} ChaseClockActivation_t;

void update_chase_clock(ChaseClockActivation_t *cc);
void update_time_vals(ChaseClockActivation_t *cc);

void init_chase_clock(ChaseClockActivation_t *cc)
{
	cc->func = (ActivationFunc) update_chase_clock;
	cc->last_run = clock_time_us();
	cc->secs = 0;
	cc->time = initTime;

	board_buffer_init(&cc->bbuf);
	board_buffer_push(&cc->bbuf, 0);

	schedule_us((Time) 1, (Activation*) cc);
}

void update_chase_clock(ChaseClockActivation_t *cc)
{
	cc->last_run += (Time) 1000000;
	schedule_absolute(cc->last_run, (Activation*) cc);

	cc->secs += 1;
	update_time_vals(cc);
	
	char buf[9];
	int_to_string2(buf,   2, 0, cc->time.hour);
	int_to_string2(buf+2, 2, 2, cc->time.min + cc->time.randoffmin);
#ifdef PRINT_DAYDATE
	strcpy(buf+4, daystrings[cc->time.day]);
	int_to_string2(buf+6, 2, 1, cc->time.date);
#else
	int_to_string2(buf+4, 2, 1, cc->time.sec);
	int_to_string2(buf+6, 2, 1, cc->time.randoffmin);
#endif	
	/* Writes string to the bitmap */
	ascii_to_bitmap_str(cc->bbuf.buffer, 8, buf);
	board_buffer_draw(&cc->bbuf);
}

void update_time_vals(ChaseClockActivation_t *cc)
{
	ChaseTime_t *tptr = &cc->time;
	
	tptr->sec += 1;
	if (tptr->sec > 59) {
		tptr->min += 1;
		tptr->sec = 0;
	}
	if (tptr->min > 59) {
		tptr->hour += 1;
		tptr->min = 0;
		if (tptr->hour > 11) {
			if (tptr->pm == TRUE) {
				tptr->pm = FALSE;
				tptr->date += 1;
				tptr->day += 1;
			}
			else
				tptr->pm = TRUE;
		}
	}
	if (tptr->hour > 12)
		tptr->hour = 1;
	if (tptr->date > days_in_month[tptr->month]) {
		tptr->date = 1;
		tptr->month += 1;
		if (tptr->month > DEC)
			tptr->month = JAN;
	}
	if (tptr->day > SAT)
		tptr->day = SUN;
	/* Leap years not handled yet */
	
	/* At specified time change the offset value based on total seconds */
//	if ((tptr->hour == SET_RAND_HOUR) && (tptr->min == SET_RAND_MIN) && (tptr->sec == SET_RAND_SEC))

	// Every 10 seconds after the appointed time, change the offset
	if ((tptr->hour >= SET_RAND_HOUR) && (tptr->min >= SET_RAND_MIN) && (tptr->sec % 10 == 0))
		tptr->randoffmin = (cc->secs + (cc->secs >> 2) + (cc->secs >> 4)) % RAND_OFFSET_LIMIT;
}

int main()
{
	hal_init();
	hal_init_rocketpanel(bc_chaseclock);
	board_buffer_module_init();

	// start clock with 10 msec resolution
	init_clock((Time) 10000, TIMER1);

	// initialize our internal state
	ChaseClockActivation_t cca;
	init_chase_clock(&cca);

	cpumon_main_loop();
	return 0;
}
