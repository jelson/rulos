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

#include "rulos.h"
#include "morse.h"

static void missa_hal_init();
static void missa_hal_set_led(uint8_t led_num, uint8_t onoff);

///////////////////////////

#ifdef SIM

char led0_state = 'o';
char led1_state = 'o';

static void print_led_state() {
  printf("\rleds:   %c    %c     ", led0_state, led1_state);
  fflush(stdout);
}

static void missa_hal_init() {
  printf("\n");
  print_led_state();
}

static void missa_hal_set_led(uint8_t led_num, uint8_t onoff) {
  char new_state;
  
  if (onoff) {
    new_state = '*';
  } else {
    new_state = 'o';
  }

  if (led_num == 0) {
    led0_state = new_state;
  } else {
    led1_state = new_state;
  }

  print_led_state();
}

#else  // SIM

#include "hardware.h"

#define LED0_PIN GPIO_B2
#define LED1_PIN GPIO_B3

static void missa_hal_init()
{
  gpio_make_output(LED0_PIN);
  gpio_make_output(LED1_PIN);
}

static void missa_hal_set_led(uint8_t led_num, uint8_t onoff) {
  if (led_num == 0) {
    gpio_set_or_clr(LED0_PIN, onoff);
  } else {
    gpio_set_or_clr(LED1_PIN, onoff);
  }
}

#endif // SIM

////////////////////////////////////

static void morse_toggle_func_trampoline(const uint8_t onoff) {
  missa_hal_set_led(0, onoff);
}

static void start_birthday_morse(void* data);

static void start_birthday_morse_after_delay() {
  schedule_us(5000000 /* 5 sec */, start_birthday_morse, NULL);
}

static void start_birthday_morse(void* data) {
  emit_morse("happy birthday missa", 170000,
             morse_toggle_func_trampoline,
             start_birthday_morse_after_delay);
}

int main()
{
  hal_init();
  missa_hal_init();
  init_clock(10000, TIMER0);

  start_birthday_morse(NULL);

  cpumon_main_loop();
}
