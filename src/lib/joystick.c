#include "rocket.h"
#include "joystick.h"

// Using 30k resistor:
//    X direction -- adc1, full left = 1020, center=470, right=305
//          left trigger   900 on/800 off
//          right trigger  320 on/350 off
//    Y direction -- adc0, full up   = 1020, center=470, down=300

#ifndef SIM
# include "hardware.h"
# define JOYSTICK_TRIGGER_GPIO GPIO_D4
#endif
#define JOYSTICK_ADC_SCAN_RATE 10000




// this equation works assuming 42k resistors and a 0..1023 adc
static int8_t adc_to_100scale(uint16_t adc)
{
	// jonh "fixes" div-by-zero without understanding eqn:
	if (adc==0) { adc = 1; }

	int32_t retval = ((uint32_t) 84*1024 / (uint32_t) adc - 84) - 100;

	if (retval < -99)
		return -99;
	if (retval > 99)
		return 99;
	return retval;
}

#define ACTUATION_THRESHOLD 50
#define RELEASE_THRESHOLD   30
// Return value: a bitvector of JOYSTICK_ constants with histeresis.
// Numeric values of thruster positions are also given in the two OUT
// parameters.
void joystick_poll(JoystickState_t *js)
{
	js->x_pos =  adc_to_100scale(hal_read_adc(js->x_adc_channel));
	js->y_pos = -adc_to_100scale(hal_read_adc(js->y_adc_channel));
	
	if (js->x_pos < -ACTUATION_THRESHOLD)
		js->state |= JOYSTICK_LEFT;
	if (js->x_pos > -RELEASE_THRESHOLD)
		js->state &= ~(JOYSTICK_LEFT);
	if (js->x_pos > ACTUATION_THRESHOLD)
		js->state |= JOYSTICK_RIGHT;
	if (js->x_pos < RELEASE_THRESHOLD)
		js->state &= ~(JOYSTICK_RIGHT);
		

	if (js->y_pos > ACTUATION_THRESHOLD)
		js->state |= JOYSTICK_UP;
	if (js->y_pos < RELEASE_THRESHOLD)
		js->state &= ~(JOYSTICK_UP);
	if (js->y_pos < -ACTUATION_THRESHOLD)
		js->state |= JOYSTICK_DOWN;
	if (js->y_pos > -RELEASE_THRESHOLD)
		js->state &= ~(JOYSTICK_DOWN);

#ifndef SIM
	if (gpio_is_clr(JOYSTICK_TRIGGER_GPIO))
		js->state |= JOYSTICK_TRIGGER;
	else
		js->state &= ~(JOYSTICK_TRIGGER);
#endif
}

void joystick_init(JoystickState_t *js)
{
#ifndef SIM
	gpio_make_input(JOYSTICK_TRIGGER_GPIO);
#endif
	hal_init_adc(JOYSTICK_ADC_SCAN_RATE);
	hal_init_adc_channel(js->x_adc_channel);
	hal_init_adc_channel(js->y_adc_channel);
}
