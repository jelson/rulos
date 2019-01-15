#include "motors.h"
#include "core/clock.h"
#include "leds.h"
#include "rudder.h"

#ifndef SIM

#include "core/hardware.h"

///////////// motors //////////////////////////////////////

#define MOTORS_CONTROL_PIN GPIO_D7

void motors_init(MotorState *motors) {
  gpio_make_output(MOTORS_CONTROL_PIN);

  TCCR2A = 0 | _BV(COM2A1)  // non-inverting PWM
           | _BV(WGM21)     // Fast PWM, top=0xff
           | _BV(WGM20)     // Fast PWM, top=0xff
      ;

  TCCR2B = 0 | _BV(CS20)  // no prescaling
      ;

  OCR2A = 0;
}

// Set motor power; 0=off, 0xff = full power
void motors_set_power(MotorState *motors, uint8_t power) {
  motors->desired_power = power;
  OCR2A = power;
}

#define MOTOR_TEST_PERIOD_US 10000000

static void motors_test(MotorState *motors) {
  schedule_us(MOTOR_TEST_PERIOD_US, (ActivationFuncPtr)motors_test, motors);

  switch (motors->test_mode) {
    case 0:
      motors_set_power(motors, 40);
      motors->test_mode = 1;
      break;
    case 1:
      motors_set_power(motors, 0);
      motors->test_mode = 0;
      break;
  }

#if 0
	case 0:
		motors_set_power(motors, motors->desired_power + 1);
		if (motors->desired_power == 0xff) {
			motors->test_mode = 1;
		}
		break;

	case 1:
		motors_set_power(motors, motors->desired_power - 1);
		if (motors->desired_power == 0) {
			motors->test_mode = 0;
		}
		break;
	}
#endif
}

void motors_test_mode(MotorState *motors) {
  motors->desired_power = 0;
  motors->test_mode = 0;
  schedule_us(1, (ActivationFuncPtr)motors_test, motors);
}

#else
void motors_init(MotorState *motors) {}
void motors_set_power(MotorState *motors, uint8_t power) {}
void motors_test_mode(MotorState *motors) {}
#endif  // SIM
