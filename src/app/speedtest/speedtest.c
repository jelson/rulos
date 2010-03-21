//#include <avr/boot.h>
#include <avr/io.h>
//#include <avr/interrupt.h>
//#include <util/delay_basic.h>
//#include <inttypes.h>
//#include <stdio.h>
//#include <string.h>


#include "rocket.h"
#include "hardware.h"

int main()
{
	uint8_t data_array[128];
	uint8_t next[128];
	uint16_t i;

	for (i = 0; i < sizeof(data_array); i++)
	{
		switch (i % 4){
		case 0:
			data_array[i] = 0;
			break;
		case 1:
			data_array[i] = 0b01010101;
			break;
		case 2:
			data_array[i] = 0b11111111;
			break;
		case 3:
			data_array[i] = 0b10101010;
			break;
		}
		next[i] = i+1;
	}
	next[63] = 0;
	
	DDRC = 0xff;
	i = 0;
//	uint8_t *last = &data_array[64];
//	uint8_t *ip = data_array;
	while (1)
	{
		PORTC = data_array[i];
		i = next[i];
	}
}

