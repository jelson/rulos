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

#include "core/morse.h"
#include "core/rulos.h"

static void missa_hal_init();
static void missa_hal_set_led(uint8_t led_num, uint8_t onoff);

///////////////////////////

#ifdef SIM

char led0_state = 'x';
char led1_state = 'x';

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

#ifdef RULOS_ARM
#define LED0_PIN GPIO0_00
#define LED1_PIN GPIO0_01
#else
#define LED0_PIN GPIO_B0
#define LED1_PIN GPIO_B2
#endif
static void missa_hal_init() {
  gpio_make_output(LED0_PIN);
  gpio_make_output(LED1_PIN);
}

static void missa_hal_set_led(uint8_t led_num, uint8_t onoff) {
  if (led_num == 0) {
    gpio_set_or_clr(LED0_PIN, !onoff);
  } else {
    gpio_set_or_clr(LED1_PIN, !onoff);
  }
}

#endif  // SIM

////////////////////////////////////

static void morse_toggle_func_trampoline(const uint8_t onoff) {
  missa_hal_set_led(0, onoff);
}

static void start_birthday_morse(void* data);

static void start_birthday_morse_after_delay() {
  schedule_us(5000000 /* 5 sec */, start_birthday_morse, NULL);
}

static void start_birthday_morse(void* data) {
  emit_morse("happy birthday missa", 170000, morse_toggle_func_trampoline,
             start_birthday_morse_after_delay);
}

/////////////////////////////////////////

const int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, -1};

typedef struct {
  uint8_t prime_num;
  uint8_t symbol_num;
  uint8_t is_keyed;
} prime_state_t;

prime_state_t prime_state;

static void do_prime_count(void* data);

static void init_prime_counting() {
  prime_state.prime_num = 0;
  prime_state.symbol_num = 0;
  prime_state.is_keyed = 0;

  do_prime_count(NULL);
}

static void prime_delay(uint8_t time_quanta) {
  schedule_us((uint32_t)210000 * time_quanta, do_prime_count, NULL);
}

static void do_prime_count(void* data) {
  if (prime_state.is_keyed) {
    missa_hal_set_led(1, 0);
    prime_state.is_keyed = 0;

    prime_state.symbol_num++;

    if (prime_state.symbol_num < primes[prime_state.prime_num]) {
      // End of this symbol. Wait the inter-symbol delay.
      prime_delay(1);
      return;
    }

    prime_state.prime_num++;
    prime_state.symbol_num = 0;
    if (primes[prime_state.prime_num] != -1) {
      // End of this prime. Wait the inter-prime delay.
      prime_delay(3);
      return;
    }

    // End of sequence. Reset for next sequence.
    prime_state.prime_num = 0;
    prime_delay(10);
    return;
  } else {
    // Turn on keying and wait symbol delay.
    missa_hal_set_led(1, 1);
    prime_state.is_keyed = 1;
    prime_delay(1);
  }
}

int main() {
  hal_init();
  missa_hal_init();
  missa_hal_set_led(0, 0);
  missa_hal_set_led(1, 0);
  init_clock(10000, TIMER0);

  start_birthday_morse(NULL);
  init_prime_counting();

  cpumon_main_loop();
}
