#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
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

/****************************************************************************/

#ifndef SIM

#include "hardware.h"
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>


#define LED0		GPIO_C1
#define LED1		GPIO_C3
#define LED2		GPIO_C5
#define POT			GPIO_C4
#define SERVO		GPIO_B1
#define PUSHBUTTON	GPIO_B0

#define BUTTON_SCAN_PERIOD	20000
#define BUTTON_REFRACTORY_PERIOD	40000

#define SERVO_PERIOD	20000
#define SERVO_PULSE_MIN_US	350
#define SERVO_PULSE_MAX_US	2000
#define POT_ADC_CHANNEL 4
#define OPT0_ADC_CHANNEL 2
#define OPT1_ADC_CHANNEL 0
#define QUADRATURE_PERIOD 1000
#define SYSTEM_CLOCK 1000

/****************************************************************************
 Dealing with jitter. I can think of four techniques to stabilize the pulse
 width.

 1. Use hardware PWM generation.
 	+ very stable pulse & period
	- low resolution: 1/20 * 1024 => 51 values
	- have to move system clock to an 8-bit timer => software postscaling
 2. Use a hardware CTC timer to establish pulse width.
 	+ very stable pulse
	- have to move system clock to a different timer
		or solder PWM to OC2 line, which means leaving servo attached
		to programming header pin
	- all remaining timers 8-bit timer => software postscaling
	- doesn't stabilize period
 3. Eliminate cli/sei pairs
 	+ no hardware constraints or clock conflict
 	- success requires eliminating ALL cli/sei -- software maintenance hassle
	- doesn't stabilize period
 4. p0wn the processor and busy-wait during software pulse generation
 	+ no constraints on scheduler structure
 	- blocks processor for 2ms at a time
	- doesn't stabilize period

 *--------------------------------------------------------------------------*/

typedef struct s_servo {
} ServoAct;

void servo_set_pwm(ServoAct *servo, uint16_t pwm_width);

void init_servo(ServoAct *servo)
{
	gpio_make_output(SERVO);

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

	servo_set_pwm(servo, 500);
}

void servo_set_pwm(ServoAct *servo, uint16_t pwm_width)
{
	OCR1A = SERVO_PULSE_MIN_US+
		(pwm_width /
			(65535/(SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)));
}

/****************************************************************************/

typedef struct s_quadrature {
	ActivationFunc func;
	uint16_t threshhold;
	uint8_t oldState;
	int16_t position;
	int8_t sign;
} Quadrature;

static void quadrature_handler(Quadrature *quad);

void init_quadrature(Quadrature *quad)
{
	quad->func = (ActivationFunc) quadrature_handler;
	quad->threshhold = 400;
	quad->oldState = 0;
	quad->position = 0;
	schedule_us(1, (Activation*) quad);
}

#define XX	0	/* invalid transition */
static int8_t quad_state_machine[16] = {
	 0,	//0000
	+1,	//0001
	-1,	//0010
	XX,	//0011
	-1,	//0100
	 0,	//0101
	XX,	//0110
	+1,	//0111
	+1,	//1000
	XX,	//1001
	 0,	//1010
	-1,	//1011
	XX,	//1100
	-1,	//1101
	+1,	//1110
	 0	//1111
};
	
static void quadrature_handler(Quadrature *quad)
{
	schedule_us(QUADRATURE_PERIOD, (Activation*) quad);

	r_bool c0 = hal_read_adc(OPT0_ADC_CHANNEL) > quad->threshhold;
	r_bool c1 = hal_read_adc(OPT1_ADC_CHANNEL) > quad->threshhold;
	uint8_t newState = (c1<<1) | c0;
	uint8_t transition = (quad->oldState<<2) | newState;
	int8_t delta = quad_state_machine[transition];
	quad->position += delta * quad->sign;
	quad->oldState = newState;
}

int16_t quad_get_position(Quadrature *quad)
{
	return quad->position;
}

void quad_set_position(Quadrature *quad, int16_t pos)
{
	quad->position = pos;
}

/****************************************************************************/

// atmega8 manual pages 22-23
// sigh. already defined in avr .h file.

#if 0
void eeprom_write(uint16_t address, uint8_t data)
{
	while (EECR & (1<<EEWE))
		{}

	EEAR = address;
	EEDR = data;
	EECR |= (1<<EEMWE);
	EECR |= (1<<EEWE);
}

uint8_t eeprom_read(uint16_t address)
{
	while (EECR & (1<<EEWE))
		{}

	EEAR = address;
	EECR |= (1<<EERE);
	return EEDR;
}

void eeprom_write_block(uint16_t ee_dest, uint8_t *src, uint16_t length)
{
	uint16_t off;
	for (off=0; off<length; off++)
	{
		eeprom_write(ee_dest+off, src[off]);
	}
}

void eeprom_read_block(uint8_t *dest, uint16_t ee_src, uint16_t length)
{
	uint16_t off;
	for (off=0; off<length; off++)
	{
		dest[off] = eeprom_read(ee_src+off);
	}
}
#endif

/****************************************************************************/

r_bool get_pushbutton()
{
	gpio_make_input(PUSHBUTTON);
	return gpio_is_clr(PUSHBUTTON);
}

/****************************************************************************/

typedef struct s_config {
	uint8_t magic;	// to detect a valid eeprom record
	int16_t doorMax;
	int16_t servoPos[2];
	int8_t quadSign;
} Config;

#define EEPROM_CONFIG_BASE 0
#define CONFIG_MAGIC 0xc3

r_bool init_config(Config *config)
{
	r_bool valid;

	if (get_pushbutton())
	{
		// hold button on boot to clear memory
		valid = FALSE;
	}
	else
	{
		eeprom_read_block((void*) config, EEPROM_CONFIG_BASE, sizeof(*config));
		valid = (config->magic == CONFIG_MAGIC);
	}
	
	if (!valid)
	{
		// install default meaningless values
		config->magic = CONFIG_MAGIC;
		config->doorMax = 60;
		config->servoPos[0] = 0;
		config->servoPos[1] = 212;
		config->quadSign = 1;
	}
	return valid;
}

uint16_t config_clip(Config *config, Quadrature *quad)
{
	int16_t newDoor = quad_get_position(quad);
	newDoor = min(newDoor, config->doorMax);
	newDoor = max(newDoor, 0);
	quad_set_position(quad, newDoor);
	return newDoor;
}

void config_update_servo(Config *config, ServoAct *servo, uint16_t doorPos)
{
	// Linearly interpolate
	int32_t servoPos = (config->servoPos[1]-config->servoPos[0]);
	servoPos *= doorPos;
	servoPos /= config->doorMax;
	servoPos += config->servoPos[0];

	servo_set_pwm(servo, (uint16_t) servoPos*(65535/1024));
}

void config_write_eeprom(Config *config)
{
	/* note arg order is src, dest; reversed from avr eeprom_read_block */
	eeprom_write_block((void*) config, EEPROM_CONFIG_BASE, sizeof(*config));
}

/****************************************************************************/

typedef enum {
	cm_run,
	cm_setLeft,
	cm_setRight,
} ControlMode;

typedef struct s_control_act {
	ActivationFunc func;
	ControlMode mode;
	Quadrature quad;
	Config config;
	ServoAct servo;
	uint16_t pot_servo;
	struct s_control_event_handler {
		UIEventHandlerFunc handler_func;
		struct s_control_act *controlAct;
	} handler;
} ControlAct;

static void control_update(ControlAct *ctl);
static UIEventDisposition control_handler(
	struct s_control_event_handler *handler, UIEvent evt);

void init_control(ControlAct *ctl)
{
	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);

	ctl->func = (ActivationFunc) control_update;
	init_quadrature(&ctl->quad);
	r_bool valid = init_config(&ctl->config);
	if (valid)
	{
		ctl->quad.sign = ctl->config.quadSign;
		ctl->mode = cm_run;
	}
	else
	{
		ctl->mode = cm_setLeft;
	}
	init_servo(&ctl->servo);
	ctl->pot_servo = 0;
		// not important; gets immediately overwritten.
		// Just trying to have good constructor hygiene.
	ctl->handler.handler_func = (UIEventHandlerFunc) control_handler;
	ctl->handler.controlAct = ctl;
	schedule_us(100, (Activation*) ctl);
}

static void control_servo_from_pot(ControlAct *ctl)
{
	uint32_t pot = hal_read_adc(POT_ADC_CHANNEL);
	ctl->pot_servo = pot;
	servo_set_pwm(&ctl->servo, ctl->pot_servo*(65535/1024));
}

static void control_update(ControlAct *ctl)
{
	schedule_us(100, (Activation*) ctl);

	uint8_t blink_rate = 0;

	// display quad info
	int16_t pos = quad_get_position(&ctl->quad) & 0x03;
	gpio_set_or_clr(LED2, !(pos==2));
	gpio_set_or_clr(LED1, !(pos==1));
	gpio_set_or_clr(LED0, !(pos==0));

	switch (ctl->mode)
	{
	case cm_run:
		{
		blink_rate = 0;
		uint8_t pos = config_clip(&ctl->config, &ctl->quad);
	 	config_update_servo(&ctl->config, &ctl->servo, pos);
		break;
		}
	case cm_setLeft:
		{
		blink_rate = 18;
		// hold door at "zero"
		quad_set_position(&ctl->quad, 0);

		control_servo_from_pot(ctl);
		
		break;
		}
	case cm_setRight:
		{
		blink_rate = 17;
		// let door slide to learn doorMax

		control_servo_from_pot(ctl);

		break;
		}
	}
	if (blink_rate!=0)
	{
		gpio_set_or_clr(LED0, (clock_time_us()>>blink_rate) & 1);
	}
}

static UIEventDisposition control_handler(
	struct s_control_event_handler *handler, UIEvent evt)
{
	ControlAct *control = handler->controlAct;
	switch (control->mode)
	{
	case cm_run:
		{
			control->mode = cm_setLeft;
			control->quad.sign = +1;
			break;
		}
	case cm_setLeft:
		{
			control->config.servoPos[0] = control->pot_servo;
			control->mode = cm_setRight;
			break;
		}
	case cm_setRight:
		{
			control->config.servoPos[1] = control->pot_servo;
			int8_t quad_sign = +1;
			int16_t doorMax = quad_get_position(&control->quad);
			if (doorMax < 0)
			{
				doorMax = -doorMax;
				quad_sign = -1;
			}
			control->config.doorMax = doorMax;
			control->config.quadSign = quad_sign;
			config_write_eeprom(&control->config);
			control->mode = cm_run;
			control->quad.sign = quad_sign;
			break;
		}
	}
	return uied_accepted;
}

/****************************************************************************/

typedef struct s_btn_act {
	ActivationFunc func;
	r_bool lastState;
	Time lastStateTime;
	UIEventHandler *handler;
} ButtonAct;

static void button_update(ButtonAct *button);

void init_button(ButtonAct *button, UIEventHandler *handler)
{
	button->func = (ActivationFunc) button_update;
	button->lastState = FALSE;
	button->lastStateTime = clock_time_us();
	button->handler = handler;

	gpio_make_input(PUSHBUTTON);

	schedule_us(1, (Activation*) button);
}

void button_update(ButtonAct *button)
{
	schedule_us(BUTTON_SCAN_PERIOD, (Activation*) button);

	r_bool buttondown = get_pushbutton();

	if (buttondown == button->lastState)
		{ return; }

	if (clock_time_us() - button->lastStateTime >= BUTTON_REFRACTORY_PERIOD)
	{
		// This state transition counts.
		if (!buttondown)
		{
			button->handler->func(button->handler, uie_right);
		}
	}

	button->lastState = buttondown;
	button->lastStateTime = clock_time_us();
}


/****************************************************************************/

typedef struct s_blink_act {
	ActivationFunc func;
	r_bool state;
} BlinkAct;

void blink_update(BlinkAct *act)
{
	schedule_us(((Time)1)<<18, (Activation*) act);

	act->state = !act->state;
	gpio_set_or_clr(LED0, act->state);
}

void init_blink(BlinkAct *act)
{
	act->func = (ActivationFunc) blink_update;
	act->state = TRUE;
	gpio_make_output(LED0);

	schedule_us(((Time)1)<<18, (Activation*) act);
}
/****************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);
	init_clock(SYSTEM_CLOCK, TIMER2);
	hal_init_adc();
	hal_init_adc_channel(POT_ADC_CHANNEL);
	hal_init_adc_channel(OPT0_ADC_CHANNEL);
	hal_init_adc_channel(OPT1_ADC_CHANNEL);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

/*
	BlinkAct blink;
	init_blink(&blink);
*/
/*
	ServoAct servo;
	init_servo(&servo);
	servo_set_pwm(&servo, 110);


	QuadTest qt;
	init_quad_test(&qt);
*/

	ControlAct control;
	init_control(&control);

	ButtonAct button;
	init_button(&button, (UIEventHandler*) &control.handler);
	
	cpumon_main_loop();

	return 0;
}

#else
int main()
{
	return 0;
}
#endif // SIM

#if 0
/****************************************************************************/
/* test code */
/****************************************************************************/

typedef struct s_quad_test {
	ActivationFunc func;
	Quadrature quad;
} QuadTest;

static void qt_update(QuadTest *qt);

void init_quad_test(QuadTest *qt)
{
	qt->func = (ActivationFunc) qt_update;

	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);

	init_quadrature(&qt->quad);

	schedule_us(20000, (Activation*) qt);
}

/* bar spacing:
 * sensors are 0.4" on center
 * they should be 1/4 or 3/4 out of phase.
 * That gives p*.25 = 0.4 or p*.75 = 0.4
 * => p = 1.6 or p = .533
 * => f = .625 or f = 1.87
 * Bar width in points = (p/2)*72: 19.188 for f=1.87
 * To make quick & dirty bar, go to
 * http://incompetech.com/graphpaper/weightedgrid/
 *   horiz line weight 19.188
 *   vert line weight 0
 *   grid spacing 1.87
 *   color black
 */

static void qt_update(QuadTest *qt)
{
	schedule_us(20000, (Activation*) qt);

/*
	gpio_set_or_clr(LED2, !(hal_read_adc(POT_ADC_CHANNEL) > 350));
*/
	//gpio_set_or_clr(LED2, (clock_time_us()>>18) & 1);

/*
	gpio_set_or_clr(LED2, !(hal_read_adc(POT_ADC_CHANNEL) > 350));
	gpio_set_or_clr(LED1, !(hal_read_adc(OPT0_ADC_CHANNEL) > 350));
	gpio_set_or_clr(LED0, !(hal_read_adc(OPT1_ADC_CHANNEL) > 350));
*/
	uint8_t v = quad_get_position(&qt->quad) & 0x03;
	gpio_set_or_clr(LED2, !(v==2));
	gpio_set_or_clr(LED1, !(v==1));
	gpio_set_or_clr(LED0, !(v==0));
	
	/*
	uint16_t v = hal_read_adc(OPT1_ADC_CHANNEL);
	gpio_set_or_clr(LED2, !((v>>9) & 1));
	gpio_set_or_clr(LED1, !((v>>8) & 1));
	gpio_set_or_clr(LED0, !((v>>7) & 1));
	*/
}


#endif // 0
