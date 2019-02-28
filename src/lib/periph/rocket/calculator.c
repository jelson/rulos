/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include <stdio.h>

#include "periph/rocket/calculator.h"
#include "periph/rocket/rocket.h"

//#define LOG_CALC 1

enum { op_add = 0, op_sub = 1, op_mul = 2, op_div = 3 };
const char *operator_strs[] = {"add", "sub", "mul", "div"};

UIEventDisposition calculator_notify(UIEventHandler *notify, UIEvent evt);
UIEventDisposition calculator_notify_internal(Calculator *calc, UIEvent evt);
void calculator_timeout_func(Calculator *calc);

void calculator_init(Calculator *calc, int board0, FocusManager *fa,
                     FetchCalcDecorationValuesIfc *fetchDecorationValuesObj) {
  calc->func = calculator_notify;
  focus_init(&calc->focus);

  int i;
  for (i = 0; i < 2; i++) {
    board_buffer_init(&calc->bbuf[i] DBG_BBUF_LABEL("calculator"));
    board_buffer_push(&calc->bbuf[i], board0 + i);
    calc->btable[i] = &calc->bbuf[i];
  }

  if (fa != NULL) {
    RectRegion calc_region = {calc->btable, 2, 1, 6};
    focus_register(fa, (UIEventHandler *)&calc->focus, calc_region,
                   "calculator");
  }

  RowRegion region0 = {&calc->bbuf[0], 0, 4};
  numeric_input_init(&calc->operands[0], region0, (UIEventHandler *)calc,
                     &calc->focus, "o1");
  RowRegion region1 = {&calc->bbuf[0], 4, 4};
  numeric_input_init(&calc->operands[1], region1, (UIEventHandler *)calc,
                     &calc->focus, "o2");
  RowRegion region2 = {&calc->bbuf[1], 0, 4};
  knob_init(&calc->op, region2, operator_strs, 4, (UIEventHandler *)calc,
            &calc->focus, "op");
  RowRegion region3 = {&calc->bbuf[1], 4, 4};
  numeric_input_init(&calc->result, region3, NULL /*unfocusable*/,
                     NULL /* no notify */, NULL /* label */);

  DecimalFloatingPoint op0 = {220, 0};
  DecimalFloatingPoint op1 = {659, 2};
  numeric_input_set_value(&calc->operands[0], op0);
  numeric_input_set_value(&calc->operands[1], op1);
  knob_set_value(&calc->op, op_div);
  calculator_notify((UIEventHandler *)calc, evt_notify);

  calc->decorationTimeout.last_activity = clock_time_us();
  calc->decorationTimeout.fetchDecorationValuesObj = fetchDecorationValuesObj;
  schedule_us(1, (ActivationFuncPtr)calculator_timeout_func, calc);
}

const char err_overflow[] = "Ovrf";
const char err_divzero[] = "div0";
const char err_negative[] = "Neg ";

UIEventDisposition calculator_notify(UIEventHandler *notify, UIEvent evt) {
  Calculator *calc = (Calculator *)notify;
  calc->decorationTimeout.last_activity = clock_time_us();

  return calculator_notify_internal(calc, evt);
}

// _internal doesn't update last_activity, so that displaying decorations
// doesn't cancel the decorations.
UIEventDisposition calculator_notify_internal(Calculator *calc, UIEvent evt) {
  assert(evt == evt_notify);
  DecimalFloatingPoint out;
  const char *error = NULL;
  DecimalFloatingPoint op0 = calc->operands[0].cur_value;
  DecimalFloatingPoint op1 = calc->operands[1].cur_value;
#if LOG_CALC
  LOG("start  op0 %3de%d o1 %3de%d", op0.mantissa, op0.neg_exponent,
      op1.mantissa, op1.neg_exponent);
#endif
  uint32_t mantissa = 0;
  switch (calc->op.selected) {
    case op_mul: {
      mantissa = ((uint32_t)op0.mantissa) * ((uint32_t)op1.mantissa);
      out.neg_exponent = op0.neg_exponent + op1.neg_exponent;
      break;
    }
    case op_div: {
      mantissa = op0.mantissa;
      while (op1.neg_exponent > 0) {
        mantissa *= 10;
        op1.neg_exponent -= 1;
      }
      if (op1.mantissa == 0) {
        error = err_divzero;
        goto done;
      }
      while (0 < mantissa && mantissa < 100000) {
        mantissa *= 10;
        op0.neg_exponent += 1;
      }
      mantissa /= ((uint32_t)op1.mantissa);
      out.neg_exponent = op0.neg_exponent;

#if LOG_CALC
      LOG("div  out %3de%d", out.mantissa, out.neg_exponent);
#endif
      break;
    }
    case op_add:
    case op_sub: {
      // get op0 to have a larger exponent (smaller neg_exponent)
      uint8_t swapped = FALSE;
      if (op0.neg_exponent > op1.neg_exponent) {
        DecimalFloatingPoint tmp = op0;
        op0 = op1;
        op1 = tmp;
        swapped = TRUE;
      }
      // now multiply mantissa to align (12 => 12.0)
      uint32_t m0 = op0.mantissa;
      uint32_t m1 = op1.mantissa;
      while (op0.neg_exponent < op1.neg_exponent) {
        m0 *= 10;
        op0.neg_exponent += 1;
      }
#if LOG_CALC
      LOG("setup2 op0 %3de%d o1 %3de%d", m0, op0.neg_exponent, m1,
          op1.neg_exponent);
#endif
      assert(op0.neg_exponent == op1.neg_exponent);
      // swap mantissas back so subtraction makes sense
      if (swapped) {
        uint32_t tmp = m0;
        m0 = m1;
        m1 = tmp;
      }
      // now operate
      switch (calc->op.selected) {
        case op_add:
          mantissa = m0 + m1;
          break;
        case op_sub:
          if (m1 > m0) {
            error = err_negative;
            goto done;
          }
          mantissa = m0 - m1;
          break;
      }
      out.neg_exponent = op0.neg_exponent;
    }
  }
#if LOG_CALC
  LOG("after  op0 %3de%d", mantissa, out.neg_exponent);
#endif
  // shift away digits until mantissa is legal
  while (mantissa >> 16) {
    if (out.neg_exponent == 0) {
      error = err_overflow;
      goto done;
    }
    mantissa /= 10;
    out.neg_exponent -= 1;

#if LOG_CALC
    LOG("16bit  op0 %3de%d", mantissa, out.neg_exponent);
#endif
  }
#if LOG_CALC
  LOG("cleanup op0 %3de%d", mantissa, out.neg_exponent);
#endif
  out.mantissa = mantissa;
  // now shift away significant digits until it can be represented
  while (dfp_draw(&out, NULL, 4, FALSE) > 4) {
    if (out.neg_exponent == 0) {
      error = err_overflow;
      goto done;
    }
    out.mantissa /= 10;
    out.neg_exponent -= 1;
#if LOG_CALC
    LOG("4char  op0 %3de%d", out.mantissa, out.neg_exponent);
#endif
  }

done:
  if (error) {
    out.mantissa = 0;
    out.neg_exponent = 2;
    numeric_input_set_value(&calc->result, out);
    numeric_input_set_msg(&calc->result, error);
  } else {
    numeric_input_set_value(&calc->result, out);
  }
  return uied_accepted;
}

#define DECORATION_TIMEOUT 7000000        /* 3 sec */
#define DECORATION_UPDATE_INTERVAL 500000 /* 0.5 sec */

void calculator_timeout_func(Calculator *calc) {
  schedule_us(DECORATION_UPDATE_INTERVAL,
              (ActivationFuncPtr)calculator_timeout_func, calc);
#if LOG_CALC
  LOG("calc: calculator_timeout_func");
#endif

  if (focus_is_active(&calc->focus)) {
    // focus means activity.
    calc->decorationTimeout.last_activity = clock_time_us();
  } else if (later_than(clock_time_us(), calc->decorationTimeout.last_activity +
                                             DECORATION_TIMEOUT)) {
#if LOG_CALC
    LOG("calc: timed out");
#endif
    // update last_activity to keep it from rolling over
    calc->decorationTimeout.last_activity =
        clock_time_us() - DECORATION_TIMEOUT - 1;

    // fetch values from source
    if (calc->decorationTimeout.fetchDecorationValuesObj != NULL) {
      DecimalFloatingPoint op0, op1;
      calc->decorationTimeout.fetchDecorationValuesObj->func(
          calc->decorationTimeout.fetchDecorationValuesObj, &op0, &op1);
      numeric_input_set_value(&calc->operands[0], op0);
      numeric_input_set_value(&calc->operands[1], op1);
      calculator_notify_internal(calc, evt_notify);
    }
  }
}
