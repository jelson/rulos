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
#include "periph/input_controller/input_controller.h"

#define LED0      GPIO_C1
#define LED1      GPIO_C3
#define LED2      GPIO_C5
#define POT       GPIO_C4
#define SERVO     GPIO_B1
#define SET_BTN   GPIO_B0
#define LEFT_BTN  GPIO_D2
#define RIGHT_BTN GPIO_D3

#define READ_BUTTON(btn) (gpio_is_clr(btn))

#define PAN_PERIOD (10000)           /* tick one PAN_RATE every 10ms */
#define PAN_RATE   (65535 / 100 / 4) /* move complete distance every 4s */

#define BUTTON_SCAN_PERIOD       20000
#define BUTTON_REFRACTORY_PERIOD 40000

#define SERVO_PERIOD       20000
#define SERVO_PULSE_MIN_US 350
#define SERVO_PULSE_MAX_US 2000
#define POT_ADC_CHANNEL    4
#define OPT0_ADC_CHANNEL   2
#define OPT1_ADC_CHANNEL   0
#define QUADRATURE_PERIOD  1000
#define ADC_PERIOD         500
#define SYSTEM_CLOCK       500

/****************************************************************************
 Dealing with jitter. I can think of four techniques to stabilize the pulse
 width.

 1. Use hardware PWM generation.
        + very stable pulse & period
        - low resolution: 1/20 * 1024 => 51 values
  ** ^ this assumption turned out to be wrong. Hardware can generate 16-bit
  ** PWM. And that works just groovily.
        - have to move system clock to an 8-bit timer => software postscaling
 2. Use a hardware CTC timer to establish pulse width.
        + very stable pulse
        - have to move system clock to a different timer
                or solder PWM to OC2 line, which means leaving servo attached
                to programming header pin
        - all remaining timers 8-bit timer => software postscaling
        - doesn't stabilize period
 3. Eliminate cli/sei pairs
        + no hardware constraints or clock conflict
        - success requires eliminating ALL cli/sei -- software maintenance
 hassle
        - doesn't stabilize period
 4. p0wn the processor and busy-wait during software pulse generation
        + no constraints on scheduler structure
        - blocks processor for 2ms at a time
        - doesn't stabilize period

 *--------------------------------------------------------------------------*/

/*
 goals:
 1. If the door isn't moving, the long-term PWM value sent to the servo
 should be a constant. A twitchy input sensor should not twitch the PWM.
        - if the commanded position is within a small delta of the current
        output, ignore the command and stick with current output. Delta
        should be 1 unit, measured in input-space clicks.
 2. If the door starts moving the other way, we should move extra distance
 to make up for backlash (slop) in the actuator mechanics.
    - always push the window around with the PWM.
 3. Buzz protection: if we've just pushed on the actuator, we should overshoot
 a little, and then, after the servo has finished actuating, back off a little
 to leave the servo somewhere it's not buzzing.
        - pwm to the edge of lash window temporarily,
        - then rest a little inside the window
*/

typedef struct s_servo {
  uint16_t desired_position;
  uint16_t rest_offset;
} ServoAct;

#define SERVO_PUSH 7
#define SERVO_REST 9

void servo_set_pwm(ServoAct *servo, uint16_t pwm_width, uint8_t push_state);

void init_servo(ServoAct *servo) {
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

  servo->desired_position = 0;  // ensure servo_set_pwm sets all fields
  servo_set_pwm(servo, 500, SERVO_PUSH);
}

#define SERVO_MAX    (0xffff)
#define SERVO_SLOP   (0x0180)
#define SERVO_OFFSET (1000) /* give room to clamp values w/ over/underflow */

void servo_set_pwm(ServoAct *servo, uint16_t desired_position,
                   uint8_t push_state) {
  int32_t output;

  if (push_state == SERVO_REST) {
    // assert(desired_position == servo->desired_position)
    output = SERVO_OFFSET + servo->desired_position + servo->rest_offset;
  } else {
    if (desired_position == servo->desired_position) {
      return;
    }

    if (desired_position < servo->desired_position) {
      output = SERVO_OFFSET + desired_position - SERVO_SLOP;
      servo->rest_offset = +SERVO_SLOP * 3 / 4;
    } else {
      output = SERVO_OFFSET + desired_position + SERVO_SLOP;
      servo->rest_offset = -SERVO_SLOP * 3 / 4;
    }
  }

  // clamp output value
  output = bound(output, SERVO_OFFSET,
                 ((int32_t)SERVO_OFFSET) + ((uint32_t)SERVO_MAX));
  output = output - SERVO_OFFSET;
  uint16_t output16 = output;

  // record state for next time
  servo->desired_position = desired_position;

  OCR1A = SERVO_PULSE_MIN_US +
          (output16 / (65535 / (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)));
}

/****************************************************************************/

/* bar spacing:
 * sensors are 0.4" on center
 * they should be 1/4 or 3/4 out of phase.
 * That gives p*.25 = 0.4 or p*.75 = 0.4
 * => p = 1.6 or p = .533
 * => f = .625 or f = 1.87
 * Bar width in points = (p/2)*72: 19.188 for f=1.87
 * To make quick & dirty bar, go to
 * http://incompetech.com/graphpaper/lined/
 *   horiz line weight 19.188
 *   grid spacing 1.87
 *   color black
 */

typedef struct s_quadrature {
  uint16_t threshhold;
  uint8_t oldState;
  int16_t position;
  int8_t sign;
} Quadrature;

static void quadrature_handler(Quadrature *quad);

void init_quadrature(Quadrature *quad) {
  quad->threshhold = 400;
  quad->oldState = 0;
  quad->position = 0;
  schedule_us(1, (ActivationFuncPtr)quadrature_handler, quad);
}

#define XX 0 /* invalid transition */
static int8_t quad_state_machine[16] = {
    0,   // 0000
    +1,  // 0001
    -1,  // 0010
    XX,  // 0011
    -1,  // 0100
    0,   // 0101
    XX,  // 0110
    +1,  // 0111
    +1,  // 1000
    XX,  // 1001
    0,   // 1010
    -1,  // 1011
    XX,  // 1100
    -1,  // 1101
    +1,  // 1110
    0    // 1111
};

static void quadrature_handler(Quadrature *quad) {
  schedule_us(QUADRATURE_PERIOD, (ActivationFuncPtr)quadrature_handler, quad);

  bool c0 = hal_read_adc(OPT0_ADC_CHANNEL) > quad->threshhold;
  bool c1 = hal_read_adc(OPT1_ADC_CHANNEL) > quad->threshhold;
  uint8_t newState = (c1 << 1) | c0;
  uint8_t transition = (quad->oldState << 2) | newState;
  int8_t delta = quad_state_machine[transition];
  quad->position += delta * quad->sign;
  quad->oldState = newState;
}

int16_t quad_get_position(Quadrature *quad) {
  return quad->position;
}

void quad_set_position(Quadrature *quad, int16_t pos) {
  quad->position = pos;
}

/****************************************************************************/

typedef struct s_config {
  uint8_t magic;  // to detect a valid eeprom record
  int16_t doorMax;
  int16_t servoPos[2];
  int8_t quadSign;
} Config;

#define EEPROM_CONFIG_BASE 4
#define CONFIG_MAGIC       0xc3
void config_read_eeprom(Config *config);

bool init_config(Config *config) {
  config_read_eeprom(config);

  if (config->magic != CONFIG_MAGIC || !READ_BUTTON(SET_BTN))
  // button-down on boot => ignore eeprom
  // Not clear this is doing anything, since the light doesn't
  // start blinking until I release the button, which is also
  // what it does when you push/release after boot. Oh well.
  // Hopefully the code will not break in a way requiring this
  // recovery. :v)
  {
    // install default meaningless values
    config->magic = CONFIG_MAGIC;
    config->doorMax = 60;
    config->servoPos[0] = 0;
    config->servoPos[1] = 212;
    config->quadSign = 1;
    return FALSE;
  }
  return TRUE;
}

void config_read_eeprom(Config *config) {
  eeprom_read_block((void *)config, (void *)EEPROM_CONFIG_BASE,
                    sizeof(*config));
}

uint16_t config_clip(Config *config, Quadrature *quad) {
  int16_t newDoor = quad_get_position(quad);
  newDoor = r_min(newDoor, config->doorMax);
  newDoor = r_max(newDoor, 0);
  quad_set_position(quad, newDoor);
  return newDoor;
}

void config_update_servo(Config *config, ServoAct *servo, uint16_t doorPos,
                         uint8_t push_state) {
  // Linearly interpolate
  int32_t servoPos = (config->servoPos[1] - config->servoPos[0]);
  servoPos *= doorPos;
  servoPos /= config->doorMax;
  servoPos += config->servoPos[0];

  servo_set_pwm(servo, (uint16_t)servoPos, push_state);
}

void config_write_eeprom(Config *config) {
  /* note arg order is src, dest; reversed from avr eeprom_read_block */
  eeprom_write_block((void *)config, (void *)EEPROM_CONFIG_BASE,
                     sizeof(*config));
}

/****************************************************************************/

#define IDLE_TIME ((Time)1000000) /* idle after 1 sec */

typedef enum {
  cm_run,
  cm_setLeft,
  cm_setRight,
} ControlMode;

struct s_control_act;

struct s_control_event_handler {
    UIEventHandlerFunc handler_func;
    struct s_control_act *controlAct;
};

typedef struct s_control_act {
  ControlMode mode;
  Quadrature quad;
  Config config;
  ServoAct servo;
  uint16_t pot_servo;
  struct s_control_event_handler handler;
#if 0
  uint8_t msg;
#endif
  Time lastPanTime;
  uint16_t pan_pos;

  // idle checks
  Time last_movement_time;
  Time last_position;
} ControlAct;

static void control_update(ControlAct *ctl);
static UIEventDisposition control_handler(
    struct s_control_event_handler *handler, UIEvent evt);

void init_control(ControlAct *ctl) {
  gpio_make_input_enable_pullup(LEFT_BTN);
  gpio_make_input_enable_pullup(RIGHT_BTN);

  gpio_make_output(LED0);
  gpio_make_output(LED1);
  gpio_make_output(LED2);

  gpio_set(LED0);
  gpio_set(LED1);
  gpio_set(LED2);

  init_quadrature(&ctl->quad);
  bool valid = init_config(&ctl->config);
  if (valid) {
    ctl->quad.sign = ctl->config.quadSign;
    ctl->mode = cm_run;
  } else {
    ctl->mode = cm_setLeft;
  }
  init_servo(&ctl->servo);
  ctl->pot_servo = 0;
  // not important; gets immediately overwritten.
  // Just trying to have good constructor hygiene.
  ctl->handler.handler_func = (UIEventHandlerFunc)control_handler;
  ctl->handler.controlAct = ctl;
  ctl->lastPanTime = clock_time_us();
  ctl->pan_pos = 0x8fff;
  ctl->last_movement_time = 0;
  ctl->last_position = 0;
  schedule_us(100, (ActivationFuncPtr)control_update, ctl);
}

#if USE_POT
static void control_servo_from_pot(ControlAct *ctl) {
  uint32_t pot = hal_read_adc(POT_ADC_CHANNEL);
  ctl->pot_servo = pot;
  servo_set_pwm(&ctl->servo, ctl->pot_servo * (65535 / 1024), SERVO_PUSH);
}
#else
static void control_servo_from_pan(ControlAct *ctl) {
  servo_set_pwm(&ctl->servo, ctl->pan_pos, SERVO_PUSH);
}
#endif

#define IGNORE_THRESH (2)

static uint8_t control_run_mode(ControlAct *ctl) {
  Time now = clock_time_us();
  Time since_movement = now - ctl->last_movement_time;
  bool idle;
  if (since_movement < 0) {
    // deal with clock wraparound -- preserve idleness
    ctl->last_movement_time = now - IDLE_TIME - 1;
    idle = TRUE;
  } else if (since_movement > IDLE_TIME) {
    idle = TRUE;
  } else {
    idle = FALSE;
  }

  uint16_t new_pos = config_clip(&ctl->config, &ctl->quad);
  bool significant_movement = new_pos <= ctl->last_position - IGNORE_THRESH ||
                              ctl->last_position + IGNORE_THRESH <= new_pos;

  if (!idle || significant_movement) {
    if (significant_movement) {
      // don't update these unless we actually move;
      // otherwise non-idleness causes non-idleness.
      ctl->last_movement_time = now;
      ctl->last_position = new_pos;
    }
    config_update_servo(&ctl->config, &ctl->servo, new_pos, SERVO_PUSH);
  } else {
    // else quiescent or idle & quiet "enough":
    // let servo rest; don't update anything until a "real" change arrives
    config_update_servo(&ctl->config, &ctl->servo, ctl->last_position,
                        SERVO_REST);
  }

  return idle ? 0 : 15;
}

static void control_update(ControlAct *ctl) {
  schedule_us(100, (ActivationFuncPtr)control_update, ctl);

  if (clock_time_us() - ctl->lastPanTime > PAN_PERIOD) {
    ctl->lastPanTime = clock_time_us();
    if (READ_BUTTON(LEFT_BTN)) {
      ctl->pan_pos = (ctl->pan_pos < PAN_RATE) ? 0 : ctl->pan_pos - PAN_RATE;
    } else if (READ_BUTTON(RIGHT_BTN)) {
      ctl->pan_pos = ((SERVO_MAX - ctl->pan_pos) < PAN_RATE)
                         ? SERVO_MAX
                         : ctl->pan_pos + PAN_RATE;
    }
  }

  uint8_t blink_rate = 0;

  // display quad info
  int16_t pos = quad_get_position(&ctl->quad) & 0x03;

  gpio_set_or_clr(LED0, !(pos == 0));

#define DISP_POSITION 1

#if DISP_BTNS
  gpio_set_or_clr(LED2, !READ_BUTTON(RIGHT_BTN));
  gpio_set_or_clr(LED1, !READ_BUTTON(LEFT_BTN));
#endif

#if DISP_POSITION
  gpio_set_or_clr(LED2, !(pos == 2));
  gpio_set_or_clr(LED1, !(pos == 1));
#endif

#if DISP_MSG
  gpio_set_or_clr(LED2, !((ctl->msg >> 1) & 1));
  gpio_set_or_clr(LED1, !((ctl->msg >> 0) & 1));
#endif

  switch (ctl->mode) {
    case cm_run: {
      blink_rate = control_run_mode(ctl);
      break;
    }
    case cm_setLeft: {
      blink_rate = 18;
      // hold door at "zero"
      quad_set_position(&ctl->quad, 0);

      control_servo_from_pan(ctl);

      break;
    }
    case cm_setRight: {
      blink_rate = 17;
      // let door slide to learn doorMax

      control_servo_from_pan(ctl);

      break;
    }
  }
  if (blink_rate != 0) {
    gpio_set_or_clr(LED0, (clock_time_us() >> blink_rate) & 1);
  }
}

#if 0
static void control_read_test(ControlAct *control)
{
	Config read_test;
	config_read_eeprom(&read_test);
	control->msg = (read_test.magic == CONFIG_MAGIC)
		? 3 : 2;
}
#endif

static UIEventDisposition control_handler(
    struct s_control_event_handler *handler, UIEvent evt) {
  ControlAct *control = handler->controlAct;
  switch (control->mode) {
    case cm_run: {
      control->mode = cm_setLeft;
      control->quad.sign = +1;
      break;
    }
    case cm_setLeft: {
#if USE_POT
      control->config.servoPos[0] = control->pot_servo * (65535 / 1024);
#else
      control->config.servoPos[0] = control->pan_pos;
#endif
      control->mode = cm_setRight;
      break;
    }
    case cm_setRight: {
#if USE_POT
      control->config.servoPos[1] = control->pot_servo * (65535 / 1024);
#else
      control->config.servoPos[1] = control->pan_pos;
#endif
      int8_t quad_sign = +1;
      int16_t doorMax = quad_get_position(&control->quad);
      if (doorMax < 0) {
        doorMax = -doorMax;
        quad_sign = -1;
      }
      control->config.doorMax = doorMax;
      control->config.quadSign = quad_sign;
      config_write_eeprom(&control->config);

      control->mode = cm_run;
      control->quad.sign = quad_sign;
      break;
    }
  }
  return uied_accepted;
}

/****************************************************************************/

typedef struct s_btn_act {
  bool lastState;
  Time lastStateTime;
  UIEventHandler *handler;
} ButtonAct;

static void button_update(ButtonAct *button);

void init_button(ButtonAct *button, UIEventHandler *handler) {
  button->lastState = FALSE;
  button->lastStateTime = clock_time_us();
  button->handler = handler;

  gpio_make_input_enable_pullup(SET_BTN);

  schedule_us(1, (ActivationFuncPtr)button_update, button);
}

void button_update(ButtonAct *button) {
  schedule_us(BUTTON_SCAN_PERIOD, (ActivationFuncPtr)button_update, button);

  bool buttondown = READ_BUTTON(SET_BTN);

  if (buttondown == button->lastState) {
    return;
  }

  if (clock_time_us() - button->lastStateTime >= BUTTON_REFRACTORY_PERIOD) {
    // This state transition counts.
    if (!buttondown) {
      button->handler->func(button->handler, uie_right);
    }
  }

  button->lastState = buttondown;
  button->lastStateTime = clock_time_us();
}

/****************************************************************************/

typedef struct s_blink_act {
  bool state;
} BlinkAct;

void blink_update(BlinkAct *act) {
  schedule_us(((Time)1) << 18, (ActivationFuncPtr)blink_update, act);

  act->state = !act->state;
  gpio_set_or_clr(LED0, act->state);
}

void init_blink(BlinkAct *act) {
  act->state = TRUE;
  gpio_make_output(LED0);

  schedule_us(((Time)1) << 18, (ActivationFuncPtr)blink_update, act);
}

/****************************************************************************/

int main() {
  rulos_hal_init();
  init_clock(SYSTEM_CLOCK, TIMER2);
  hal_init_adc(ADC_PERIOD);
  hal_init_adc_channel(POT_ADC_CHANNEL);
  hal_init_adc_channel(OPT0_ADC_CHANNEL);
  hal_init_adc_channel(OPT1_ADC_CHANNEL);


  /*
          BlinkAct blink;
          init_blink(&blink);
  */
  /*
          ServoAct servo;
          init_servo(&servo);
          servo_set_pwm(&servo, 110, SERVO_PUSH);


          QuadTest qt;
          init_quad_test(&qt);

          gpio_make_input_enable_pullup(SET_BTN);
          gpio_make_output(LED0);
          gpio_make_output(LED1);
          gpio_make_output(LED2);

          uint8_t *p = (uint8_t *) 17;
          uint8_t v = eeprom_read_byte(p);
          gpio_set_or_clr(LED0, !((v>>0)&1));
          gpio_set_or_clr(LED1, !((v>>1)&1));
          gpio_set_or_clr(LED2, !((v>>2)&1));
          v+=1;
          eeprom_write_byte(p, v);
  */
  ControlAct control;
  init_control(&control);

  ButtonAct button;
  init_button(&button, (UIEventHandler *)&control.handler);

  scheduler_run();

  return 0;
}
