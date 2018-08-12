#include "core/rulos.h"
#include "hardware.h"

#define F_CPU 8000000UL
#include <util/delay.h>

#define NUM_COLS 48

#define COLLATCH_CLK  GPIO_B0
#define COLLATCH_OE   GPIO_B1
#define COLLATCH_LE   GPIO_B2
#define COLLATCH_SDI  GPIO_B3

int main()
{
	int8_t colNum = 1;

	gpio_make_output(COLLATCH_CLK);
	gpio_make_output(COLLATCH_OE);
	gpio_make_output(COLLATCH_LE);
	gpio_make_output(COLLATCH_SDI);

	gpio_clr(COLLATCH_OE);

	while (1) {
		int8_t i;

		gpio_clr(COLLATCH_SDI);
		for (i = 0; i < colNum-1; i++) {
			gpio_set(COLLATCH_CLK);
			gpio_clr(COLLATCH_CLK);
		}
		gpio_set(COLLATCH_SDI);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);

		int8_t post = NUM_COLS - colNum;

		gpio_clr(COLLATCH_SDI);
		for (i = 0; i < post; i++) {
			gpio_set(COLLATCH_CLK);
			gpio_clr(COLLATCH_CLK);
		}

		gpio_set(COLLATCH_LE);
		gpio_clr(COLLATCH_LE);

		_delay_ms(500);

		if (colNum == NUM_COLS)
			colNum = 1;
		else
			colNum++;
	}

}
