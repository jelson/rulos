#include "hardware.h"
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "periph/servo/servo.h"
#include "periph/uart/uart.h"

//////////////////////////////////////////////////////////////////////////////

#define LED0		GPIO_D7
#define LED1		GPIO_C0
#define LED2		GPIO_C1
#define LED3		GPIO_C2
#define LED4		GPIO_C3

#define MARK_POINT_ENABLED 0

void mark_point_init()
{
#if MARK_POINT_ENABLED
	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);
	gpio_make_output(LED3);
	gpio_make_output(LED4);
#endif // MARK_POINT_ENABLED
}

void mark_point(uint8_t val)
{
#if MARK_POINT_ENABLED
	gpio_set_or_clr(LED0, (((val>>0)&0x1)!=0));
	gpio_set_or_clr(LED1, (((val>>1)&0x1)!=0));
	gpio_set_or_clr(LED2, (((val>>2)&0x1)!=0));
	gpio_set_or_clr(LED3, (((val>>3)&0x1)!=0));
	gpio_set_or_clr(LED4, (((val>>4)&0x1)!=0));
#endif // MARK_POINT_ENABLED
}

