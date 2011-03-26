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
#include <string.h>
#include <math.h>

#include "rocket.h"
#include "6matrix.h"

#ifndef SIM
#define F_CPU 8000000L
#include <util/delay.h>
#endif


void makeBarGraph(uint8_t numCols, uint8_t *output)
{
	uint8_t *currPtr = output;

	memset(output, 0, NUM_COLUMN_BYTES_1BIT);
	
	while (numCols >= 8) {
		*currPtr = 0xff;
		currPtr++;
		numCols -= 8;
	}

	// for the last one, shift in as many ones as there are columns
	uint8_t i;
	for (i = 0; i < numCols; i++) {
		*currPtr |= ((0b10000000) >> i);
	}
}

int main(int argc, char *argv[])
{
	util_init();
	//	hal_init(bc_audioboard);
	hal_ledmatrix_init(&argc, &argv);

	uint8_t testPattern[NUM_COLUMN_BYTES_1BIT];
	uint8_t numCols = 0;

	while (1) {
		makeBarGraph(numCols, testPattern);
		hal_ledmatrix_setRow(testPattern, 0);
		_delay_ms(350);
		numCols++;
		if (numCols > NUM_COLUMNS)
			numCols = 0;
	}

#if 0
	uint8_t rowNum = 0, lastRowNum = 0;
	while (1) {
		row_power(lastRowNum, 0);
		row_power(rowNum, 1);
		lastRowNum = rowNum;
		rowNum = (rowNum + 1) % NUM_ROWS;
		_delay_ms(1000);
	}
#endif

#if 0
#ifdef USE_SCHEDULER
	ma.f = (ActivationFunc) assert_next_column;
	init_clock(USEC_PER_COL, TIMER1);
	schedule_now((Activation *) &ma);
#else
	hal_start_clock_us(USEC_PER_COL, (Handler) assert_next_column, &ma, TIMER2);
#endif

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
#endif
	return 0;
}

