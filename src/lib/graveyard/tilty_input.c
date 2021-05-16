#include "graveyard/tilty_input.h"

#include "chip/avr/periph/pov/pov.h"  // for pov_paint

#define G_QUIESCENT 50
#define G_GRAVITY 100
//#define DBG_CAT_ON
#if DBG_CAT_ON
#define DBG_CAT(x) strcat(dbg_msg, x);
#else
#define DBG_CAT(x) /**/
#endif

#define TI_INPUT_DWELL_TIME (((Time)1) << 19)

#define TI_SCAN_PERIOD 20000
// some weird bug prevents us from going faster.
// scan at 5ms, and it crashes pretty reliably
// seems to have to do with serial

void remote_debug(char *msg);

static inline bool is_quiescent(int16_t value) {
  return (value > -G_QUIESCENT && value < G_QUIESCENT);
}

static inline TiltyInputState accel_to_state(Vect3D *accel) {
  bool qx = is_quiescent(accel->x);
  bool qy = is_quiescent(accel->y);
  bool qz = is_quiescent(accel->z);

  if (qx && qy && accel->z > G_GRAVITY) {
    return ti_neutral;
  }
  if (qx && qz && accel->y < G_GRAVITY) {
    return ti_up;
  }
  if (qx && qz && accel->y > G_GRAVITY) {
    return ti_down;
  }
  if (qy && qz && accel->x < G_GRAVITY) {
    return ti_left;
  }
  if (qy && qz && accel->x > G_GRAVITY) {
    return ti_right;
  }
  // some other orientation we don't care about.
  return ti_undef;
}

static inline void tia_start_leds(TiltyInputAct *tia, TiltyLEDPattern tlp) {
  tia->led_pattern = tlp;
  tia->led_anim_start = clock_time_us();
}

void tia_display_leds(TiltyInputAct *tia
#if DBG_CAT_ON
                      ,
                      char *dbg_msg
#endif  // DBG_CAT_ON
) {
  if (tia->curState == ti_up) {
    // don't paint on the LEDs; we're showing text.
    return;
  }

  switch (tia->led_pattern) {
    case tia_led_click: {
      DBG_CAT("led_click\n");
      // 1<<18 == ~1/4 sec total display time;
      // 1<<16 leaves 2 bits of animation time.
      Time td = (clock_time_us() - tia->led_anim_start) >> 16;
      switch (td) {
        case 0:
          pov_paint(0x04);
          break;
        case 1:
          pov_paint(0x0e);
          break;
        case 2:
          pov_paint(0x0e);
          break;
        case 3:
          pov_paint(0x1f);
          break;
        default:
          pov_paint(0x00);
          break;
      }
      break;
    }
    case tia_led_proposal: {
      DBG_CAT("led_proposal\n");
      // 1<<19 == ~1/2 sec total display time; (matches DWELL_TIME)
      // 1<<17 leaves 2 bits of animation time.
      Time td = (clock_time_us() - tia->led_anim_start) >> 17;
      switch (td) {
        case 0:
          pov_paint(0x10);
          break;
        case 1:
          pov_paint(0x18);
          break;
        case 2:
          pov_paint(0x1c);
          break;
        case 3:
          pov_paint(0x1e);
          break;
        default:
          pov_paint(0x1f);
          break;
      }
      break;
    }
    case tia_led_black: {
      DBG_CAT("led_black\n");
      pov_paint(0x00);
      break;
    }
  }
}

void tilty_input_update(TiltyInputAct *tia) {
  schedule_us(TI_SCAN_PERIOD, (ActivationFuncPtr)tilty_input_update, tia);
#if DBG_CAT_ON
  static char dbg_msg[200];
  dbg_msg[0] = '\0';
#endif  // DBG_CAT_ON

  if (tia->accelValue->z > G_GRAVITY * 3) {
    DBG_CAT("WHACk!\n");
  }

  TiltyInputState newState = accel_to_state(tia->accelValue);

  Time time_now = clock_time_us();

#if DBG_CAT_ON
  sprintf(dbg_msg + strlen(dbg_msg), "\n state cur: %3x new: %2x\n",
          tia->curState, newState);
#endif
  if (ti_proposed & tia->curState) {
    DBG_CAT("proposed:\n");
    if ((tia->curState & (~ti_proposed)) == newState) {
      DBG_CAT("  new==cur\n");
      // we're camping in a state. Have we been here long enough?
      Time input_duration = time_now - tia->proposed_time;
      if (input_duration > TI_INPUT_DWELL_TIME) {
        DBG_CAT("    DWELL\n");
        tia->curState = newState;
        TiltyInputState evt = newState;
        if (newState == ti_up) {
          evt = ti_enter_pov;
        }
        if (newState == ti_neutral) {
          evt = ti_exit_pov;
        }
        (tia->event_handler->func)(tia->event_handler, evt);
        tia_start_leds(tia, tia_led_click);
      }
    } else {
      DBG_CAT("  left proposed\n");
      // we left the proposed state. Go back to the original
      // state, and decide where to go next in the next sample.
      if ((tia->curState & (~ti_proposed)) == ti_neutral) {
        tia->curState = ti_up;
      } else {
        tia->curState = ti_neutral;
      }
      tia_start_leds(tia, tia_led_black);
    }
  } else {
    DBG_CAT("regular:\n");
    if (tia->curState == ti_up) {
      DBG_CAT("  cur up:\n");
      if (newState == ti_neutral) {
        DBG_CAT("    new neutral:\n");
        tia->curState = newState | ti_proposed;
        tia->proposed_time = time_now;
        tia_start_leds(tia, tia_led_proposal);
      } else {
        DBG_CAT("    nonsense:\n");
        // trying to enter some nonsense state.
        // just sit here in ti_pov.
      }
    } else if (tia->curState == ti_neutral) {
      DBG_CAT("  cur neutral:\n");
      if (newState == ti_left || newState == ti_right || newState == ti_down ||
          newState == ti_up) {
        DBG_CAT("    new lrdi:\n");
        tia->curState = newState | ti_proposed;
        tia->proposed_time = time_now;
        tia_start_leds(tia, tia_led_proposal);
      } else {
        DBG_CAT("    undef:\n");
        // trying to enter ti_undef? Meh, pretend we're still in
        // ti_neutral, so we can pass through ti_undef on our
        // way to where we're going.
      }
    } else {
      DBG_CAT("  input-bit:\n");
      // we're in an input-bit state, waiting to
      // return to ti_neutral.
      if (newState == ti_neutral) {
        DBG_CAT("    neutral:\n");
        tia->curState = ti_neutral;
      } else {
        DBG_CAT("    nonsense:\n");
        // Any other state is
        // nonsense, so we'll ignore it until we pass
        // through neutral.
      }
    }
  }

  tia_display_leds(tia
#if DBG_CAT_ON
                   ,
                   dbg_msg
#endif  // DBG_CAT_ON
  );
#if DBG_CAT_ON
  remote_debug(dbg_msg);
#endif  // DBG_CAT_ON
}

void tilty_input_init(TiltyInputAct *tia, Vect3D *accelValue,
                      UIEventHandler *event_handler) {
  tia->accelValue = accelValue;
  tia->event_handler = event_handler;

  tia->curState = ti_undef;
  tia->proposed_time = clock_time_us();

  tia->led_pattern = tia_led_black;
  tia->led_anim_start = clock_time_us();

  schedule_us(1, (ActivationFuncPtr)tilty_input_update, tia);
}
