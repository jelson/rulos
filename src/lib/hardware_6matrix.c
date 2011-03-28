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

#define USE_SCHEDULER

#include "rocket.h"
#include "6matrix.h"
#include "hardware.h"

#define F_CPU 8000000UL
#include <util/delay.h>

#define COLLATCH_CLK  GPIO_B0
#define COLLATCH_OE   GPIO_B1
#define COLLATCH_LE   GPIO_B2
#define COLLATCH_SDI  GPIO_B3

struct {
	volatile uint8_t *ddr;
	volatile uint8_t *port;
	volatile uint8_t *pin;
	uint8_t bit;
} row_power[] = {
	{ GPIO_D2 }, // 1
	{ GPIO_D1 }, // 2
	{ GPIO_D0 }, // 3
	{ GPIO_C3 }, // 4
	{ GPIO_D5 }, // 5
	{ GPIO_D6 }, // 6
	{ GPIO_D7 }, // 7
	{ GPIO_B4 }, // 8
	{ GPIO_D3 }, // 9
	{ GPIO_D4 }, // 10
	{ GPIO_B6 }, // 11
	{ GPIO_B7 }, // 12
	{ GPIO_C2 }, // 13
	{ GPIO_C1 }, // 14
	{ GPIO_C0 }, // 15
	{ GPIO_B5 }, // 16
};
#define GPIO_ROW(x) (row_power[x].ddr), (row_power[x].port), (row_power[x].pin), (row_power[x].bit)

void hal_6matrix_init()
{
	gpio_make_output(COLLATCH_CLK);
	gpio_make_output(COLLATCH_OE);
	gpio_make_output(COLLATCH_LE);
	gpio_make_output(COLLATCH_SDI);

	for (uint8_t i = 0; i < NUM_ROWS; i++) {
		gpio_make_output(GPIO_ROW(i));
		gpio_clr(GPIO_ROW(i));
	}
}

static void assertRow(uint8_t *colBytes, uint8_t oldRowNum, uint8_t newRowNum)
{
	// shift in the new data for each column
	gpio_clr(COLLATCH_LE);
	gpio_clr(COLLATCH_CLK);
	for (uint8_t byteNum = 0; byteNum < NUM_COLUMN_BYTES_1BIT; byteNum++, colBytes++) {
		uint8_t currByte = *colBytes;
		for (uint8_t i = 0; i < 8; i++) {
			gpio_set_or_clr(COLLATCH_SDI, currByte & 0b10000000);
			currByte <<= 1;
			gpio_set(COLLATCH_CLK);
			gpio_clr(COLLATCH_CLK);
		}
	}

	// turn off old row
	gpio_set(COLLATCH_OE);
	gpio_clr(GPIO_ROW(oldRowNum));

	// assert new row's columns
	gpio_set(COLLATCH_LE);
	gpio_clr(COLLATCH_LE);

	// turn on new row
	gpio_clr(COLLATCH_OE);
	gpio_set(GPIO_ROW(newRowNum));
}


void hal_6matrix_setRow(uint8_t *colBytes, uint8_t rowNum)
{
	// temporary for now.  eventually this will copy into a frame buffer
	assertRow(colBytes, 0, 0);
}
