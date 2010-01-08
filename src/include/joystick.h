#ifndef __joystick_h__
#define __joystick_h__

#ifndef _BV
# define _BV(x) (1 << (x))
#endif

#define JOYSTICK_UP           _BV(0)
#define JOYSTICK_DOWN         _BV(1)
#define JOYSTICK_LEFT         _BV(2)
#define JOYSTICK_RIGHT        _BV(3)
#define JOYSTICK_TRIGGER      _BV(4)
#define JOYSTICK_DISCONNECTED _BV(5)
typedef struct {
	// ADC channel numbers.  Should be initialized by caller before
	// joystick_init is called.
	uint8_t x_adc_channel;
	uint8_t y_adc_channel;

	// X and Y positions (from -100 to 100) of joystick, and a state
	// bitvector, valid after joystick_poll is called.
	int8_t x_pos;
	int8_t y_pos;
	uint8_t state;
} JoystickState_t;

void joystick_init(JoystickState_t *js);
void joystick_poll(JoystickState_t *js);


#endif // __joystick_h__
