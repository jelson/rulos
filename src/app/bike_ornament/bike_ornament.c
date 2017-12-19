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

#include "clock.h"
#include "hardware.h"
#include "rulos.h"
#include "util.h"

#define LED_DRIVER_SDI_L GPIO_A1
#define LED_DRIVER_SDI_R GPIO_A0
#define LED_DRIVER_CLK GPIO_A2
#define LED_DRIVER_LE GPIO_A3
#define HEADLIGHT GPIO_B2
#define TAILLIGHT GPIO_B1

#define JIFFY_TIME_US 2500

typedef struct {
  // wheels
  uint8_t light_on_l;
  uint8_t light_on_r;
  Time wheels_next_move_time;
  
  // taillight
  r_bool tail_on;
  Time tail_next_toggle_time;
} BikeState_t;

#define TAIL_ON_TIME_MS 100
#define TAIL_OFF_TIME_MS 300
#define WHEEL_PERIOD_MS 10

static inline void clock()
{
  gpio_set(LED_DRIVER_CLK);
  gpio_clr(LED_DRIVER_CLK);
}

//
// This function shifts 8 bits into our 8-bit latch.
// The stove has only outputs 0 and 1 connected, so we shift real data
// into positions 0 and 1, and turn the rest off.
// 
// Each time a bit is shifted, it takes position 0, and pushing
// 0 through n-1 into positions 1 through n.  So, we shift 6 0's in first,
// then output 1, and finally output 0.
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

int main() {
  hal_init();

  // set up output pins as drivers
  gpio_make_output(LED_DRIVER_SDI_L);
  gpio_make_output(LED_DRIVER_SDI_R);
  gpio_make_output(LED_DRIVER_CLK);
  gpio_make_output(LED_DRIVER_LE);
  gpio_make_output(HEADLIGHT);
  gpio_make_output(TAILLIGHT);

  // turn on the headlight
  gpio_set(HEADLIGHT);

  // init state
  BikeState_t bike;
  memset(&bike, 0, sizeof(bike));
  Time now = clock_time_us();
  bike.light_on_l = 0;
  bike.light_on_r = 0;
  bike.wheels_next_move_time = now;
  bike.tail_on = false;
  bike.tail_next_toggle_time = now;

  // set up periodic update
  init_clock(JIFFY_TIME_US, TIMER1);
  schedule_now((ActivationFuncPtr)bike_update, &bike);
  cpumon_main_loop();
  assert(FALSE);
}
