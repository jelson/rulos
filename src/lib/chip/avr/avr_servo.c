#include "periph/servo/servo.h"

#ifndef SIM
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "hardware.h"

#define SERVO_PERIOD 20000
#define SERVO_PULSE_MIN_US 350
#define SERVO_PULSE_MAX_US 2000

#define SERVO GPIO_B1

void servo_init() {
  gpio_make_output(SERVO);

  TCCR1A = 0 | _BV(COM1A1)  // non-inverting PWM
           | _BV(WGM11)     // Fast PWM, 16-bit, TOP=ICR1
      ;
  TCCR1B = 0 | _BV(WGM13)  // Fast PWM, 16-bit, TOP=ICR1
           | _BV(WGM12)    // Fast PWM, 16-bit, TOP=ICR1
           | _BV(CS11)     // clkio/8 prescaler => 1us clock
      ;

  ICR1 = 20000;
  OCR1A = 1500;

  servo_set_pwm(500);
}

void servo_set_pwm(uint16_t desired_position) {
  OCR1A =
      SERVO_PULSE_MIN_US +
      (desired_position / (65535 / (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)));
}
#else
void servo_init() {}
void servo_set_pwm(uint16_t desired_position) {}
#endif  // SIM
