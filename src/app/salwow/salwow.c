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
#include "clock.h"
#include "util.h"

#if 0
#include "display_controller.h"
#include "display_rtc.h"
#include "display_scroll_msg.h"
#include "display_compass.h"
#include "focus.h"
#include "labeled_display.h"
#include "display_docking.h"
#include "display_gratuitous_graph.h"
#include "numeric_input.h"
#include "input_controller.h"
#include "calculator.h"
#include "display_aer.h"
#include "hal.h"
#include "cpumon.h"
#include "idle_display.h"
#include "sequencer.h"
#include "rasters.h"
#include "pong.h"
#include "lunar_distance.h"
#include "sim.h"
#include "display_thrusters.h"
#include "network.h"
#include "remote_keyboard.h"
#include "remote_bbuf.h"
#include "remote_uie.h"
#include "control_panel.h"
#endif

/****************************************************************************/

#ifndef SIM

#include "hardware.h"
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "servo.h"
#include "uart.h"

//////////////////////////////////////////////////////////////////////////////

#define LED0		GPIO_D7
#define LED1		GPIO_C0
#define LED2		GPIO_C1
#define LED3		GPIO_C2
#define LED4		GPIO_C3

void mark_point_init()
{
	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);
	gpio_make_output(LED3);
	gpio_make_output(LED4);
}

void mark_point(uint8_t val)
{
	gpio_set_or_clr(LED0, (((val>>0)&0x1)!=0));
	gpio_set_or_clr(LED1, (((val>>1)&0x1)!=0));
	gpio_set_or_clr(LED2, (((val>>2)&0x1)!=0));
	gpio_set_or_clr(LED3, (((val>>3)&0x1)!=0));
	gpio_set_or_clr(LED4, (((val>>4)&0x1)!=0));
}

//////////////////////////////////////////////////////////////////////////////

#define SYSTEM_CLOCK 500

UartState_t uart[2];
char *test_msg[2] = { "Aa", "Bb" };
	
void send_done(void *data)
{
	mark_point(8);
	uint16_t uart_id = (uint16_t) data;
	char *msg = test_msg[uart_id];
	uart_send(&uart[uart_id], msg, strlen(msg), send_done, (void*) uart_id);
}

int main()
{
	mark_point_init();
	mark_point(1);
	hal_init();
	mark_point(2);
	init_clock(SYSTEM_CLOCK, TIMER2);
//	hal_init_adc(ADC_PERIOD);
//	hal_init_adc_channel(POT_ADC_CHANNEL);
//	hal_init_adc_channel(OPT0_ADC_CHANNEL);
//	hal_init_adc_channel(OPT1_ADC_CHANNEL);

	mark_point(3);
	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	mark_point(4);
	uart_init(&uart[0], 38400, TRUE, 0);

	uint8_t f = 0;
	while(1) {
		f+=1;
		mark_point((f&1) | 16);
		char *msg = "my dog. worms.\n";
		hal_uart_sync_send(&uart[0].handler, msg, strlen(msg));
	}

	mark_point(5);
	uart_init(&uart[1], 38400, TRUE, 1);

	servo_init();
	send_done((void*) 0);
	send_done((void*) 1);

	cpumon_main_loop();
	mark_point(7);

	return 0;
}

#else
int main()
{
	return 0;
}
#endif // SIM


