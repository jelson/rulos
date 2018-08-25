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

#define TAIL_ON_TIME_MS  100
#define TAIL_OFF_TIME_MS 300
#define WHEEL_PERIOD_MS  10
#define ANIM_TIME_SEC    5

/////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/clock.h"
#include "core/rulos.h"
#include "core/util.h"
#include "hardware.h"

#define LED_DRIVER_SDI_L GPIO_A1
#define LED_DRIVER_SDI_R GPIO_A0
#define LED_DRIVER_CLK GPIO_A2
#define LED_DRIVER_LE GPIO_A3
#define BUTTON GPIO_B2
#define LED_DRIVER_POWER GPIO_B0
#define TAILLIGHT GPIO_B1

#define JIFFY_TIME_US 2500

typedef struct {
  // timeout
  Time shutoff_time;

  // wheels
  uint8_t light_on_l;
  uint8_t light_on_r;
  Time wheels_next_move_time;
  
  // taillight
  r_bool tail_on;
  Time tail_next_toggle_time;
} BikeState_t;

BikeState_t bike;

void bike_sleep();
void bike_wake(BikeState_t* bike);

ISR(INT0_vect) {
  bike_wake(&bike);
}

static inline void clock()
{
  gpio_set(LED_DRIVER_CLK);
  gpio_clr(LED_DRIVER_CLK);
}

// This function shifts 16 bits into the two 16-bit latches. They have
// separate data lines, but share a clock line, so in each cycle we set both
// data lines separately and then effectively clock them together.
static void shift_in_config(BikeState_t* bike)
{
  gpio_clr(LED_DRIVER_LE);
  gpio_clr(LED_DRIVER_CLK);

  // Shift in the data.
  for (int8_t i = 15; i >= 0; i--) {
    gpio_set_or_clr(LED_DRIVER_SDI_L, (bike->light_on_l == i));
    gpio_set_or_clr(LED_DRIVER_SDI_R, (bike->light_on_r == i));
    clock();
  }

  // Disable output; enable latch
  gpio_set(LED_DRIVER_LE);

  // Enable output
  gpio_clr(LED_DRIVER_LE);

  // tidy up -- not actually necessary, but makes logic analyzer output easier
  // to read
  gpio_clr(LED_DRIVER_SDI_L);
  gpio_clr(LED_DRIVER_SDI_R);
}

static void bike_update(BikeState_t* bike) {
  schedule_us(JIFFY_TIME_US, (ActivationFuncPtr)bike_update, bike);
  Time now = clock_time_us();

#if 0
  if (gpio_is_clr(BUTTON)) {
    bike_wake(bike);
  }
#endif
  
  // If we've reached the end of the animation time, shut everything down.
  if (later_than(now, bike->shutoff_time)) {
    bike_sleep();
    return;
  }
  
  // If we've reached the next blink state for the tail light, toggle it
  if (later_than(now, bike->tail_next_toggle_time)) {
    if (bike->tail_on) {
      gpio_clr(TAILLIGHT);
      bike->tail_on = false;
      bike->tail_next_toggle_time = now + (TAIL_OFF_TIME_MS * (uint32_t)1000);
    } else {
      gpio_set(TAILLIGHT);
      bike->tail_on = true;
      bike->tail_next_toggle_time = now + (TAIL_ON_TIME_MS * (uint32_t)1000);
    }
  }

  // If we've reached the next update time for the bicycle lights, update them
  if (later_than(now, bike->wheels_next_move_time)) {
    bike->light_on_l = (bike->light_on_l + 1) % 16;
    bike->light_on_r = (bike->light_on_r + 1) % 16;
    shift_in_config(bike);
    bike->wheels_next_move_time = now + (WHEEL_PERIOD_MS * (uint32_t)1000);
  }
}

void bike_wake(BikeState_t* bike) {
  Time now = clock_time_us();
  bike->light_on_l = 0;
  bike->light_on_r = 0;
  bike->wheels_next_move_time = now;
  bike->tail_on = false;
  bike->tail_next_toggle_time = now;
  bike->shutoff_time = now + time_sec(ANIM_TIME_SEC);

  gpio_clr(LED_DRIVER_POWER);
}

void bike_sleep() {
  gpio_set(LED_DRIVER_POWER);
  //hal_deep_sleep();
}

int main() {
  hal_init();

  init_clock(JIFFY_TIME_US, TIMER1);

  // set up output pins as drivers
  gpio_make_output(LED_DRIVER_SDI_L);
  gpio_make_output(LED_DRIVER_SDI_R);
  gpio_make_output(LED_DRIVER_CLK);
  gpio_make_output(LED_DRIVER_LE);
  gpio_make_output(LED_DRIVER_POWER);
  gpio_make_output(TAILLIGHT);

  gpio_make_input_enable_pullup(BUTTON);

  // Configure INT0 to generate interrupts at low level.
  MCUCR &= ~(_BV(ISC01) | _BV(ISC00));

  // Configure INT0 to generate interrupts.
  GIMSK |= _BV(INT0);
  sei();
  
  // Init bike state.
  memset(&bike, 0, sizeof(bike));
  bike_wake(&bike);

  // Set up periodic update.
  schedule_now((ActivationFuncPtr)bike_update, &bike);

  // Loop forever.
  cpumon_main_loop();
  assert(FALSE);
}
