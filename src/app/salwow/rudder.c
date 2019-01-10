#include "rudder.h"
#include "core/clock.h"
#include "leds.h"

#ifndef SIM

#include "hardware.h"

#define RUDDER_SERVO_PIN GPIO_D5

#define RUDDER_SERVO_MIN_US 1000
#define RUDDER_SERVO_MAX_US 2000
#define RUDDER_API_MIN (-99)
#define RUDDER_API_MAX (+99)

// position is -100 (left) to +100 (right).  0 is center.
void rudder_set_angle(RudderState *rudder, int8_t position) {
  rudder->desired_position = position;

  OCR1A =
      RUDDER_SERVO_MIN_US + (((uint16_t)position - RUDDER_API_MIN) *
                             ((RUDDER_SERVO_MAX_US - RUDDER_SERVO_MIN_US + 1) /
                              (uint16_t)(RUDDER_API_MAX - RUDDER_API_MIN + 1)));
}

#define RUDDER_TEST_PERIOD_US 20000
#define RUDDER_WAIT_PERIODS 100

static void rudder_test(RudderState *rudder) {
  schedule_us(RUDDER_TEST_PERIOD_US, (ActivationFuncPtr)rudder_test, rudder);

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
        rudder_set_angle(rudder, rudder->desired_position - 1);
      }
      break;
    case 1:
      if (rudder->desired_position >= 0) {
        rudder->test_mode = 4;
        rudder->next_mode = 2;
      } else {
        rudder_set_angle(rudder, rudder->desired_position + 1);
      }
      break;
    case 2:
      if (rudder->desired_position >= RUDDER_API_MAX) {
        rudder->test_mode = 4;
        rudder->next_mode = 3;
      } else {
        rudder_set_angle(rudder, rudder->desired_position + 1);
      }
      break;
    case 3:
      if (rudder->desired_position <= 0) {
        rudder->test_mode = 4;
        rudder->next_mode = 0;
      } else {
        rudder_set_angle(rudder, rudder->desired_position - 1);
      }
      break;

    case 4:
      if (++(rudder->delay_timer) >= RUDDER_WAIT_PERIODS) {
        rudder->delay_timer = 0;
        rudder->test_mode = rudder->next_mode;
        leds_green(TRUE);
      } else {
        leds_green(FALSE);
      }
      break;
  }
}

void rudder_test_mode(RudderState *rudder) {
  rudder->test_mode = 0;
  rudder->delay_timer = 0;
  schedule_us(1, (ActivationFuncPtr)rudder_test, rudder);
}

void rudder_init(RudderState *rudder) {
  gpio_make_output(RUDDER_SERVO_PIN);

  TCCR1A = 0 | _BV(COM1A1)  // non-inverting PWM
           | _BV(WGM11)     // Fast PWM, 16-bit, TOP=ICR1
      ;
  TCCR1B = 0 | _BV(WGM13)  // Fast PWM, 16-bit, TOP=ICR1
           | _BV(WGM12)    // Fast PWM, 16-bit, TOP=ICR1
           | _BV(CS11)     // clkio/8 prescaler => 1us clock
      ;

  ICR1 = 20000;
  OCR1A = 1500;

  rudder->desired_position = 0;  // ensure servo_set_pwm sets all fields
  rudder->test_mode = 0;
  rudder_set_angle(rudder, 0);
}

#else  // SIM
void rudder_init(RudderState *rudder) {}
void rudder_set_angle(RudderState *rudder, int8_t position) {}
#endif
