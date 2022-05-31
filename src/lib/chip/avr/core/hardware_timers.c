/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * hardware_timers.c: Hardware timer setup for AVR line.
 */

#ifdef PRESCALE_TEST

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FALSE 0
uint32_t hardware_f_cpu;

#else  // !PRESCALE_TEST

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>

#include "core/hardware.h"
#include "core/rulos.h"

void null_handler(void *data) {
}

clock_handler_t timer0_handler = null_handler;
clock_handler_t timer1_handler = null_handler;
clock_handler_t timer2_handler = null_handler;

void *timer0_data = NULL;
void *timer1_data = NULL;
void *timer2_data = NULL;

#if defined(MCU328_line) || defined(MCU1284_line)
ISR(TIMER0_COMPA_vect)
#elif defined(MCUtiny84_line) || defined(MCUtiny85_line)
ISR(TIM0_COMPA_vect)
#elif defined(MCU8_line)
static inline void unused_function_0()
#else
#error hardware-specific timer code needs help!
#endif
{
  timer0_handler(timer0_data);
}

#if defined(MCU328_line) || defined(MCU1284_line) || defined(MCU8_line)
ISR(TIMER1_COMPA_vect)
#elif defined(MCUtiny84_line) || defined(MCUtiny85_line)
ISR(TIM1_COMPA_vect)
#else
#error hardware-specific timer code needs help!
#endif
{
  timer1_handler(timer1_data);
}

#if defined(MCU8_line)
ISR(TIMER2_COMP_vect)
#elif defined(MCU328_line) || defined(MCU1284_line)
ISR(TIMER2_COMPA_vect)
#elif defined(MCUtiny84_line) || defined(MCUtiny85_line)
static inline void unused_function_2()
#else
#error hardware-specific timer code needs help!
#endif
{
  timer2_handler(timer2_data);
}

uint32_t f_cpu_values[] = {
    (1000000),
    (2000000),
    (4000000),
    (8000000),
};

uint32_t hardware_f_cpu;

void init_f_cpu() {
#ifdef CRYSTAL
  hardware_f_cpu = CRYSTAL;
#else
#if defined(MCU8_line)
  // read fuses to determine clock frequency
  uint8_t cksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0x0f;
  hardware_f_cpu = f_cpu_values[cksel - 1];
#elif defined(MCU328_line) || defined(MCU1284_line) || \
    defined(MCUtiny84_line) || defined(MCUtiny85_line)
  // If we decide to do variable clock rates on 328p, see page 37
  // for prescale configurations.
  CLKPR = 0x80;
  CLKPR = 0;
  hardware_f_cpu = (uint32_t)8000000;
#else
#error Hardware-specific clock code needs help
#endif
#endif  // CRYSTAL
}

#endif  // PRESCALE_TEST

/*
 *
 * Based on http://sunge.awardspace.com/binary_clock/binary_clock.html
 *
 * Use timer/counter1 to generate clock ticks for events.
 *
 * For >=1ms interval,
 * Using CTC mode (mode 4) with 1/64 prescaler.
 * For smaller intervals (where a 16MHz clock won't
 * overflow a 16-bit register), we use prescaler 1.
 *
 * 64 counter ticks takes 64/F_CPU seconds.  Therefore, to get an s-second
 * interrupt, we need to take one every s / (64/F_CPU) ticks.
 *
 * To avoid floating point, do computation in usec rather than seconds:
 * take an interrupt every us / (prescaler / F_CPU / 1000000) counter ticks,
 * or, (us * F_CPU) / (prescaler * 1000000).
 * We move around the constants in the calculations below to
 * keep the intermediate results fitting in a 32-bit int.
 */

static const uint8_t timer0_prescaler_bits[] = {0xff, 0,  3,    6,
                                                8,    10, 0xff, 0xff};
static const uint8_t timer1_prescaler_bits[] = {0xff, 0,  3,    6,
                                                8,    10, 0xff, 0xff};
static const uint8_t timer2_prescaler_bits[] = {0xff, 0, 3, 5, 6, 7, 8, 10};
typedef struct {
  const uint8_t *prescaler_bits;
  const uint8_t ocr_bits;
} TimerDef;
static const TimerDef timer0 = {timer0_prescaler_bits, 8};
static const TimerDef timer1 = {timer1_prescaler_bits, 16};
static const TimerDef timer2 = {timer2_prescaler_bits, 8};

static void find_prescaler(uint32_t req_us_per_period, const TimerDef *timerDef,
                           uint32_t *out_us_per_period,
                           uint8_t *out_cs,   // prescaler selection
                           uint16_t *out_ocr  // count limit
) {
#define HS_FACTOR ((uint32_t)120)
  // Units: 120hs = 1us.
  // jelson changed from 16 to 48 because of my 12mhz crystal
  // 3/4/2011: jelson changed from 48 to 120 because of my 20mhz crystal.
  // LCM[1,8,12,20] = 120

  uint8_t cs;
  for (cs = 1; cs < 8; cs++) {
    if (timerDef->prescaler_bits[cs] == 0xff) {
      continue;
    }
    uint32_t hs_per_cpu_tick = (HS_FACTOR * 1000000) / hardware_f_cpu;
    uint32_t hs_per_prescale_tick = hs_per_cpu_tick
                                    << (timerDef->prescaler_bits[cs]);
    uint32_t prescale_tick_per_period =
        ((req_us_per_period * HS_FACTOR) / hs_per_prescale_tick) + 1;
    uint32_t max_prescale_ticks = (((uint32_t)1) << timerDef->ocr_bits) - 1;

#ifdef USI_SERIAL_DEBUG
    char buf[50];
    sprintf(buf, "C%dH=%ldP=%xH=%ldI=%ldJ=%ldM=%ld", cs, hardware_f_cpu,
            timerDef->prescaler_bits[cs], hs_per_cpu_tick, hs_per_prescale_tick,
            prescale_tick_per_period, max_prescale_ticks);
    usi_serial_send(buf);
#endif

    if (prescale_tick_per_period > max_prescale_ticks) {
      // go try a bigger prescaler
      continue;
    }
    *out_us_per_period =
        (prescale_tick_per_period * hs_per_prescale_tick) / HS_FACTOR;
    *out_cs = cs;
    *out_ocr = prescale_tick_per_period;
    return;
  }
  assert(FALSE);  // might need a software scaler
}

#ifndef PRESCALE_TEST

uint32_t hal_start_clock_us(uint32_t us, clock_handler_t handler, void *data,
                            uint8_t timer_id) {
  extern uint8_t g_hal_initted;
  assert(g_hal_initted == HAL_MAGIC);
  uint32_t actual_us_per_period = 0;
  // may not equal what we asked for, because of prescaler rounding.
  uint8_t cs = 0;
  uint16_t ocr = 0;

  // disable interrupts
  cli();

  switch (timer_id) {
#if defined(MCU328_line) || defined(MCU1284_line) || \
    defined(MCUtiny84_line) || defined(MCUtiny85_line)
    case TIMER0: {
      find_prescaler(us, &timer0, &actual_us_per_period, &cs, &ocr);

      uint8_t tccr0b = 0;
      tccr0b |= (cs & 4) ? _BV(CS02) : 0;
      tccr0b |= (cs & 2) ? _BV(CS01) : 0;
      tccr0b |= (cs & 1) ? _BV(CS00) : 0;

      timer0_handler = handler;
      timer0_data = data;

      OCR0A = ocr;
      TCCR0A = _BV(WGM01);  // CTC Mode 2 (interrupt on count-up)
      TCCR0B = tccr0b;

      /* enable output-compare int. */
#if defined(MCUtiny85_line)
      TIMSK |= _BV(OCIE0A);
#else
      TIMSK0 |= _BV(OCIE0A);
#endif

      /* reset counter */
      TCNT0 = 0;
      break;
    }
#endif

#if defined(MCU8_line) || defined(MCU328_line) || defined(MCU1284_line) || \
    defined(MCUtiny84_line)
    case TIMER1: {
      find_prescaler(us, &timer1, &actual_us_per_period, &cs, &ocr);

      uint8_t tccr1b = _BV(WGM12);  // CTC Mode 4 (interrupt on count-up)
      tccr1b |= (cs & 4) ? _BV(CS12) : 0;
      tccr1b |= (cs & 2) ? _BV(CS11) : 0;
      tccr1b |= (cs & 1) ? _BV(CS10) : 0;

      timer1_handler = handler;
      timer1_data = data;

      OCR1A = ocr;
      TCCR1A = 0;
      TCCR1B = tccr1b;

      /* enable output-compare int. */
#if defined(MCU8_line)
      TIMSK |= _BV(OCIE1A);
#elif defined(MCU328_line) || defined(MCU1284_line) || defined(MCUtiny84_line)
      TIMSK1 |= _BV(OCIE1A);
#else
#error hardware-specific timer code needs help!
#endif
      /* reset counter */
      TCNT1 = 0;
      break;
    }
#endif

#if defined(MCU8_line) || defined(MCU328_line) || defined(MCU1284_line)
    case TIMER2: {
      find_prescaler(us, &timer2, &actual_us_per_period, &cs, &ocr);

      uint8_t tccr2a = _BV(WGM21);  // CTC Mode 2 (interrupt on count-up)
      uint8_t tccr2b = 0;
      tccr2b |= (cs & 4) ? _BV(CS22) : 0;
      tccr2b |= (cs & 2) ? _BV(CS21) : 0;
      tccr2b |= (cs & 1) ? _BV(CS20) : 0;

      timer2_handler = handler;
      timer2_data = data;

#if defined(MCU8_line)
      OCR2 = ocr;
      TCCR2 = tccr2a | tccr2b;
      TIMSK |= _BV(OCIE2); /* enable output-compare int. */
#elif defined(MCU328_line) || defined(MCU1284_line)
      OCR2A = ocr;
      TCCR2A = tccr2a;
      TCCR2B = tccr2b;
      TIMSK2 |= _BV(OCIE2A); /* enable output-compare int. */
#else
#error hardware-specific timer code needs help!
#endif

      /* reset counter */
      TCNT2 = 0;
      break;
    }
#endif

    default:
      assert(FALSE);
      break;
  }

  /* Enable interrupts (regardless of if they were on before or not) */
  sei();

  return actual_us_per_period;
}

/*
 * How long since the last interval timer?
 * 0 == the current interval just started.
 * 9999 = the next interval is about to start.
 */
uint16_t hal_elapsed_tenthou_intervals() {
  return (10000 * (uint32_t)TCNT1) / OCR1A;
}

bool hal_clock_interrupt_is_pending() {
#if defined(MCU8_line)
  return (TIFR & OCF1A) != 0;
#elif defined(MCU328_line)
  return (TIFR1 & OCF1A) != 0;
#else
  return false;
#endif
}

// Speed up or slow down the clock by a certain
// ratio, expressed in parts per million
//
// A ratio of 0 will keep the clock unchanged.
// Positive ratios will make the clock tick faster.
// Negative ratios will make the clock tick slower.
//
void hal_speedup_clock_ppm(int32_t ratio) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();

  uint16_t new_ocr1a = OCR1A;
  int32_t adjustment = new_ocr1a;

  adjustment *= -ratio;
  adjustment /= 1000000;
  new_ocr1a += (uint16_t)adjustment;

  OCR1A = new_ocr1a;
  if (TCNT1 >= new_ocr1a)
    TCNT1 = new_ocr1a - 1;

  hal_end_atomic(old_interrupts);
}

void hal_delay_ms(uint16_t __ms) {
  // copied from util/delay.h
  uint16_t __ticks;
  double __tmp = ((hardware_f_cpu) / 4e3) * __ms;
  if (__tmp < 1.0)
    __ticks = 1;
  else if (__tmp > 65535) {
    //      __ticks = requested delay in 1/10 ms
    __ticks = (uint16_t)(__ms * 10.0);
    while (__ticks) {
      // wait 1/10 ms
      _delay_loop_2(((hardware_f_cpu) / 4e3) / 10);
      __ticks--;
    }
    return;
  } else
    __ticks = (uint16_t)__tmp;
  _delay_loop_2(__ticks);
}

#else  // PRESCALE_TEST

int main(int argc, char *argv[]) {
  uint32_t us_per_period;
  uint8_t cs;
  uint16_t ocr;

  if (argc != 4) {
    printf("Usage: %s <cpuMhz> <timerNumber> <desiredUsec>\n", argv[0]);
    exit(1);
  }

  hardware_f_cpu = atoi(argv[1]) * 1000000;
  int16_t timerNum = atoi(argv[2]);
  int32_t period = atoi(argv[3]);

  if (timerNum < 0 || timerNum > 2) {
    printf("Invalid timer number\n");
    exit(1);
  }

  find_prescaler(period,
                 timerNum == 0 ? &timer0 : (timerNum == 1 ? &timer1 : &timer2),
                 &us_per_period, &cs, &ocr);

  printf("cpu speed (hz): %d\n", hardware_f_cpu);
  printf("us_per_period: %d\n", us_per_period);
  printf("cs: %d\n", cs);
  printf("ocr: %d\n", ocr);
  return 0;
}

#endif  // PRESCALE_TEST
