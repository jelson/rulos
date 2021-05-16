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

#include "core/hardware.h"
#include "core/rulos.h"

#define LEDR GPIO_A7
#define LEDG GPIO_B0
#define LEDB GPIO_B1
#define LEDW GPIO_B2

#define FADER_KEY        GPIO_A3
#define FADER_KEY_PULLUP (1 << PORTA3)

#define KNOB_HUE        0
#define KNOB_LIGHTNESS0 1
#define KNOB_LIGHTNESS1 2

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  uint16_t hue;
  uint16_t lightness0;
  uint16_t lightness1;
  uint8_t fader_key;
} Inputs;

void inputs_init(Inputs* inputs) {
  // note I'm NOT using init_adc(); it adds a scheduled function,
  // and we're not using any of rulos scheduling for this device.
  reg_set(&ADCSRA, ADEN);  // enable ADC
  //	reg_set(&ADCSRB, ADLAR); // enable left-aligned mode for 8-bit reads

  hal_init_adc_channel(KNOB_HUE);
  hal_init_adc_channel(KNOB_LIGHTNESS0);
  hal_init_adc_channel(KNOB_LIGHTNESS1);
  gpio_make_input_enable_pullup(FADER_KEY);
}

//----------------------------------------------------------------------
// unexposed gunk copied out of lib/hardware_adc.c :vP
//
static void adc_busy_wait_conversion() {
  reg_set(&ADCSRA, ADSC);
  while (reg_is_set(&ADCSRA, ADSC)) {
  }
}

static uint16_t read_adc_raw(uint8_t adc_channel) {
  ADMUX = adc_channel;
  adc_busy_wait_conversion();
  // superstition:
  adc_busy_wait_conversion();

  //	return ADCH;	// 8-bit variant
  uint16_t newval = ADCL;
  newval |= (((uint16_t)ADCH) << 8);
  return newval;
}
//
//----------------------------------------------------------------------

void inputs_sample(Inputs* inputs) {
  inputs->hue = read_adc_raw(KNOB_HUE);
  inputs->lightness0 = read_adc_raw(KNOB_LIGHTNESS0);
  inputs->lightness1 = read_adc_raw(KNOB_LIGHTNESS1);

  inputs->fader_key = gpio_is_clr(FADER_KEY);
}

//////////////////////////////////////////////////////////////////////////////
// Output control

typedef struct {
  uint8_t low;
  uint8_t off_cycle;
} ControlChannel;

typedef struct {
  ControlChannel r;
  ControlChannel g;
  ControlChannel b;
  ControlChannel w;
} ControlTable;

inline void channel_configure(ControlChannel* channel, uint8_t value) {
  channel->low = value & 0x7f;
  channel->off_cycle = (value >> 7) & 1;
}

void control_init(ControlTable* table) {
  gpio_make_output(LEDR);
  gpio_make_output(LEDG);
  gpio_make_output(LEDB);
  gpio_make_output(LEDW);
}

void control_phase(ControlTable* control_table) {
  uint8_t red = control_table->r.low;
  uint8_t green = control_table->g.low;
  uint8_t blue = control_table->b.low;
  uint8_t white = control_table->w.low;

  uint8_t t;
  for (t = 0; t < 128; t++) {
    PORTA = (t + red) & 0x80;
    PORTB = ((t + green) & 0x80) >> 7 | ((t + blue) & 0x80) >> 6 |
            ((~(t + white)) & 0x80) >> 5;
  }
  PORTA = (control_table->r.off_cycle << 7) | FADER_KEY_PULLUP;
  PORTB = control_table->g.off_cycle | (control_table->b.off_cycle << 1) |
          ((1 - (control_table->w.off_cycle)) << 2);
}

//////////////////////////////////////////////////////////////////////////////

// stabilizing timer.
// Each phase should take the same total amount of time,
// so stall this phase to take the same time as an output phase.
//
// Ballpark: 128 updates * 4 channels * 16 cycles per channel
// --> 8000 cycles per half-period.
// --> 16ms period @ 1MHz, 2ms @8MHz. That should do.

// prescalar constants on attiny84.pdf page 84, table 11-9.
#define TIMER_PRESCALER (0x5) /* clkIO/1024. */
#define TIMER_COUNT     (250)

void timer_init() {
  TCCR0A = 0;
  TCCR0B = TIMER_PRESCALER;
  OCR0A = TIMER_COUNT;
}

void timer_start() {
  TCNT0 = 0;
  reg_clr(&TIFR0, OCF0A);
}

void timer_wait() {
  while (reg_is_clr(&TIFR0, OCF0A)) {
  }
}

//////////////////////////////////////////////////////////////////////////////
// Master phases

typedef struct {
  Inputs inputs;
  uint8_t fader_enabled;
  uint8_t fader_brightness;
  uint32_t fader_elapsed_time;
  ControlTable control_table;
} App;

// From full brightness, 1<<2 takes about 4 minutes.
// Fading from dimmer settings is ugly; shall we take a 45% brightness setting
// and change it to 100% with a 55% discount?
#define FADER_ONE_DECREMENT_TIME (1 << 7)
#define APPLY_FADER(x)           (((x) > fader_discount) ? ((x)-fader_discount) : 0)

void hue_conversion(App* app) {
  Inputs* inputs = &app->inputs;
  ControlTable* control_table = &app->control_table;

  uint16_t hue = (inputs->hue * 3) >> 1;  // 0..1535
#define FWD(x) (x)
#define REV(x) (255 - (x))
  uint8_t r, g, b;
  uint8_t v;
  v = hue;
  if (hue < 768) {
    if (hue < 512) {
      if (hue < 256) {
        r = 255;
        g = FWD(hue);
        b = 0;
      } else {
        r = REV(v);
        g = 255;
        b = 0;
      }
    } else {
      r = 0;
      g = 255;
      b = FWD(v);
    }
  } else {
    if (hue < 1280) {
      if (hue < 1024) {
        r = 0;
        g = REV(v);
        b = 255;
      } else {
        r = FWD(v);
        g = 0;
        b = 255;
      }
    } else {
      r = 255;
      g = 0;
      b = REV(v);
    }
  }
  // now scale by lightness.
  // I drop 10-bit lightness by 2 bits to avoid overflow.
  uint16_t color_lightness = inputs->lightness0 >> 2;
  r = (((uint16_t)r) * color_lightness) >> 8;
  g = (((uint16_t)g) * color_lightness) >> 8;
  b = (((uint16_t)b) * color_lightness) >> 8;

  uint16_t w = ((inputs->lightness1) >> 2);

  if (inputs->fader_key) {
    // (re)start fader cycle
    app->fader_enabled = 1;
    app->fader_elapsed_time = 0;
    app->fader_brightness = 255;
  }
  if (app->fader_enabled) {
    if (app->fader_brightness == 0) {
      r = 0;
      g = 0;
      b = 0;
      w = 0;
    } else {
      if (((app->fader_elapsed_time) >> 8) > FADER_ONE_DECREMENT_TIME) {
        app->fader_elapsed_time = 0;
        app->fader_brightness -= 1;
      } else {
        app->fader_elapsed_time += app->fader_brightness;
      }
      uint8_t fader_discount = 255 - app->fader_brightness;
      r = APPLY_FADER(r);
      g = APPLY_FADER(g);
      b = APPLY_FADER(b);
      w = APPLY_FADER(w);
    }
  }

  channel_configure(&control_table->r, r);
  channel_configure(&control_table->g, g);
  channel_configure(&control_table->b, b);
  channel_configure(&control_table->w, w);
}

void app_set_clock() {
  CLKPR = 0x80;
  CLKPR = 0x00;
}

void app_init(App* app) {
  app_set_clock();
  timer_init();
  inputs_init(&app->inputs);
  app->fader_enabled = 0;
  control_init(&app->control_table);
}

void app_run(App* app) {
  while (1) {
    //		gpio_set(LEDW);
    inputs_sample(&app->inputs);
    // If the duty cycles overlap, increase this value;
    // if there's a gap, decrease it. (Or just ensure
    // that the reference bit has 50% duty...)
    volatile int i;
    for (i = 0; i < 570; i++) {
    }
    //		gpio_clr(LEDW);

    hue_conversion(app);
    //		timer_wait();

    control_phase(&app->control_table);
  }
}

int main() {
  // Next step: test the timer for the input phase.
  App app;
  app_init(&app);
  app_run(&app);
  return 0;
}
