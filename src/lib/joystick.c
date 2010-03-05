#include "rocket.h"
#include "joystick.h"

// Using 30k resistor:
//    X direction -- adc1, full left = 1020, center=470, right=305
//          left trigger   900 on/800 off
//          right trigger  320 on/350 off
//    Y direction -- adc0, full up   = 1020, center=470, down=300

#define JOYSTICK_ADC_SCAN_RATE 10000


// The joystick circuit looks like this:
//
// (Vref, Joy input) 
//      |
// (Joy output, ADC input, resistor lead 1)
//      |
// (resistor lead 2, gnd)
//
// The joystick is in series with a fixed-size resistor (R).  The joystick resistance,
// which ranges from 0 to 100kohm, is X.  Therefore the total resistance is X+R, and
// at any given moment the portion at the resistor is R/X+R.
// Since the ADC is scaled from 0..1023, and R=40Kohm, then the ADC value will be
//      ADC = 1023 * (X/X+R).
// Solving for X gives us
//      X = (40*1023) / ADC  - 40.  (X in Kohm)
// That will give us X between 0 and 100, since the joystick's range is 0.. 100Kohm.

static int8_t adc_to_100scale(uint16_t adc)
{
	// jonh "fixes" div-by-zero without understanding eqn:
	if (adc==0) { adc = 1; }

	// this solves for the resistance in kohms:
	//int32_t retval = ((int32_t) 40 * 1023) / adc - 40;
	// now map the resistance, which ranges from 0..85kohm, to -100 to 100
	//retval = (200 * retval) / 85 - 100;
	// and combine into a single equation (cheating using mathematica - jer algebra fail):
	int32_t retval = ((int32_t) 96282/adc) - 194;

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
	uint16_t x_adc = hal_read_adc(js->x_adc_channel);
	uint16_t y_adc = hal_read_adc(js->y_adc_channel);

	if (x_adc < 10 && y_adc < 10) {
		js->state = JOYSTICK_DISCONNECTED;
		return;
	}
		
	js->state &= ~(JOYSTICK_DISCONNECTED);

	js->x_pos = -adc_to_100scale(hal_read_adc(js->x_adc_channel));
	js->y_pos =  adc_to_100scale(hal_read_adc(js->y_adc_channel));

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

	if (hal_read_joystick_button())
		js->state |= JOYSTICK_TRIGGER;
	else
		js->state &= ~(JOYSTICK_TRIGGER);
}

void joystick_init(JoystickState_t *js)
{
	js->state = 0;
	hal_init_adc(JOYSTICK_ADC_SCAN_RATE);
	hal_init_adc_channel(js->x_adc_channel);
	hal_init_adc_channel(js->y_adc_channel);
	hal_init_joystick_button();
}
