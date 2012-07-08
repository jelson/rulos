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
#include "gpsinput.h"
#include "mark_point.h"
#include "uart.h"

/****************************************************************************/

#define SYSTEM_CLOCK 500

//////////////////////////////////////////////////////////////////////////////


UartState_t uart[2];
const char *_test_msg[2] = { "Aa", "Bb" };
char **test_msg = (char**) _test_msg;
	
void send_done(void *data);

void send_one(void *data)
{
	static int ctr;
	mark_point(16+((ctr++)&15));

	static char *smsg = (char*) "done x\n";
	smsg[5] = '0' + (ctr & 7);
	uart_debug_log(smsg);

	uint16_t uart_id = (uint16_t) (int) data;

#if 0
	assert(uart_id<2);
	char *msg = test_msg[uart_id];
#else
	char *msg = (char*) "hi\n";
#endif
	static char *vmsg = (char*) "ob x\n";
	vmsg[3] = '0' + (uart[uart_id].out_buf != NULL);
	uart_debug_log(vmsg);

	uart_send(&uart[uart_id], msg, strlen(msg), send_done, (void*) (int) uart_id);

//	{ char *m="hey doodle\n"; hal_uart_sync_send(&uart[0].handler, m, strlen(m)); }
}

void send_done(void *data)
{
	schedule_us(1, send_one, data);
}

void _test_sentence_done(void* data)
{
	GPSInput* gpsi = (GPSInput*) data;
#ifndef SIM
	{
		char smsg[100];
		int latint = (int) gpsi->lat;
		uint32_t latfrac = (gpsi->lat - latint)*1000000;
		float poslon = -gpsi->lon;
		int lonint = (int) poslon;
		uint32_t lonfrac = (poslon - lonint)*1000000;
		sprintf(smsg, "GPS: %d.%06ld, %d.%06ld\n", latint,latfrac,lonint, lonfrac);
		uart_debug_log(smsg);
	}
#else
	fprintf(stderr, "Read %f,%f\n", (double) gpsi->lat, (double) gpsi->lon);
#endif // SIM
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
	uart_init(&uart[1], 38400, TRUE, 1);

	{
		uart_debug_log("SALWOW up.\n");
	}

#if 0
	uint8_t f = 0;
	while(1) {
		f+=1;
		mark_point((f&1) | 16);
		char *msg = "my dog. worms.\n";
		hal_uart_sync_send(&uart[0].handler, msg, strlen(msg));
	}
#endif

#if 0
	{ char *m="hey diddle\n"; hal_uart_sync_send(&uart[0].handler, m, strlen(m)); }

	for (int i=0; i<100; i++) {
	{ char *m="x \n"; hal_uart_sync_send(&uart[0].handler, m, strlen(m)); }
	for (uint32_t j=0; j<1000000; j++) { }
	}

	servo_init();
	send_one((void*) 0);
#endif
	//send_one((void*) 1);

#if 1
	GPSInput gpsi;
	gpsinput_init(&gpsi, 1, _test_sentence_done, &gpsi);
#endif

	cpumon_main_loop();
	mark_point(7);

	return 0;
}
