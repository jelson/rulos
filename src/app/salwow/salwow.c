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
#include "hardware.h"
#include "uart.h"
#include "navigation.h"

/****************************************************************************/

#define SYSTEM_CLOCK 500

#define RUDDER_SERVO_PIN GPIO_D5
#define GREEN_LED_PIN    GPIO_D7

//////////////////////////////////////////////////////////////////////////////


typedef struct s_RudderState
{
	int8_t desired_position;

	// test mode
	int8_t test_mode;
	int8_t delay_timer;
	int8_t next_mode;

} RudderState;

#define RUDDER_SERVO_MIN_US 1000
#define RUDDER_SERVO_MAX_US 2000
#define RUDDER_API_MIN (-99)
#define RUDDER_API_MAX (+99)

// position is -100 (left) to +100 (right).  0 is center.
void rudder_set_angle(RudderState *rudder, int8_t position)
{
	rudder->desired_position = position;

	OCR1A = RUDDER_SERVO_MIN_US +
		(((uint16_t) position - RUDDER_API_MIN) *
		 ((RUDDER_SERVO_MAX_US - RUDDER_SERVO_MIN_US + 1) / (uint16_t) (RUDDER_API_MAX-RUDDER_API_MIN+1)));
}

#define RUDDER_TEST_PERIOD_US 20000
#define RUDDER_WAIT_PERIODS 100


static void rudder_test(RudderState *rudder)
{
	schedule_us(RUDDER_TEST_PERIOD_US, (ActivationFuncPtr) rudder_test, rudder);

	// sweep the rudder:
	// 0 -- center to left
	// 1 -- left to center
	// 2 -- center to right
	// 3 -- right to center
	// 4 -- wait
	switch (rudder->test_mode) {
	case 0:
		if (rudder->desired_position <= RUDDER_API_MIN) {
			rudder->test_mode = 4;
			rudder->next_mode = 1;
		} else {
			rudder_set_angle(rudder, rudder->desired_position-1);
		}
		break;
	case 1:
		if (rudder->desired_position >= 0) {
			rudder->test_mode = 4;
			rudder->next_mode = 2;
		} else {
			rudder_set_angle(rudder, rudder->desired_position+1);
		}
		break;
	case 2:
		if (rudder->desired_position >= RUDDER_API_MAX) {
			rudder->test_mode = 4;
			rudder->next_mode = 3;
		} else {
			rudder_set_angle(rudder, rudder->desired_position+1);
		}
		break;
	case 3:
		if (rudder->desired_position <= 0) {
			rudder->test_mode = 4;
			rudder->next_mode = 0;
		} else {
			rudder_set_angle(rudder, rudder->desired_position-1);
		}
		break;

	case 4:
		if (++(rudder->delay_timer) >= RUDDER_WAIT_PERIODS) {
			rudder->delay_timer = 0;
			rudder->test_mode = rudder->next_mode;
			gpio_set(GREEN_LED_PIN);
		} else {
			gpio_clr(GREEN_LED_PIN);
		}
		break;
	}
}

static void rudder_test_mode(RudderState *rudder)
{
	rudder->test_mode = 0;
	rudder->delay_timer = 0;
	schedule_us(1, (ActivationFuncPtr) rudder_test, rudder);
}


void rudder_init(RudderState *rudder)
{
	gpio_make_output(RUDDER_SERVO_PIN);

	TCCR1A = 0
		| _BV(COM1A1)		// non-inverting PWM
		| _BV(WGM11)		// Fast PWM, 16-bit, TOP=ICR1
		;
	TCCR1B = 0
		| _BV(WGM13)		// Fast PWM, 16-bit, TOP=ICR1
		| _BV(WGM12)		// Fast PWM, 16-bit, TOP=ICR1
		| _BV(CS11)			// clkio/8 prescaler => 1us clock
		;

	ICR1 = 20000;
	OCR1A = 1500;

	rudder->desired_position = 0;	// ensure servo_set_pwm sets all fields
	rudder->test_mode = 0;
	rudder_set_angle(rudder, 0);
}

//////////////// UART ///////////////////////////////////////

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

#ifndef SIM
void mathtest() { }
#else
Navigation nav;

void one_mathtest(Vector *p1)
{
	Vector p0;
	p0.x = p1->x+0.5;
	p0.y = p1->y+1.5;
	int res = navigation_compute(&nav, &p0, p1);
	fprintf(stderr, " %5d", res);
}

void mathtest()
{
	Vector w0; v_init(&w0, 3, 3);
	Vector w1; v_init(&w1, 2, -2);
	navigation_activate_leg(&nav, &w0, &w1);

	Vector p1;
#if 0
	p1.x = 1.0; p1.y = 1.5;
	one_mathtest(&p1);
#endif
#if 1
	for (p1.y = 3; p1.y>=-3; p1.y-=0.5)
	{
		fprintf(stderr, "[y %.1f]", (double) p1.y);
		for (p1.x = 0.1; p1.x<3.2; p1.x+=0.5)
		{
			one_mathtest(&p1);
		}
		fprintf(stderr, "\n");
	}
#endif
}
#endif
int main()
{
	mark_point_init();
	mark_point(1);
	hal_init();
	mark_point(2);
	init_clock(SYSTEM_CLOCK, TIMER0);

	// init green led
	gpio_make_output(GREEN_LED_PIN);

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

	RudderState rudder;
	rudder_init(&rudder);
	rudder_test_mode(&rudder);

#if 0
	mathtest();
#endif

	cpumon_main_loop();
	mark_point(7);

	return 0;
}
