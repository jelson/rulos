#include "periph/rocket/volume_control.h"

#include <stdbool.h>

#define VOLUME_DISPLAY_PERSISTENCE (4 * 1000000)

void _volume_input(InputInjectorIfc *ii, Keystroke key);
void _volume_update(VolumeControl *vc);

void volume_control_init(VolumeControl *vc, AudioClient *ac, uint8_t boardnum,
                         Keystroke vol_up, Keystroke vol_down) {
  vc->injector.iii.func = _volume_input;
  vc->injector.vc = vc;
  vc->ac = ac;

  // cache correct volume so next music play starts at the right place
  vc->cur_vol = 3;

  vc->vol_up = vol_up;
  vc->vol_down = vol_down;

  ac_change_volume(vc->ac, AUDIO_STREAM_MUSIC, vc->cur_vol);

#if DISPLAY_VOLUME_ADJUSTMENTS
  vc->lastTouch = clock_time_us() - VOLUME_DISPLAY_PERSISTENCE * 2;
  vc->visible = false;
  vc->boardnum = boardnum;
  board_buffer_init(&vc->bbuf DBG_BBUF_LABEL("volume_control"));

  schedule_us(1, (ActivationFuncPtr)_volume_update, vc);
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
}

void _volume_input(InputInjectorIfc *ii, Keystroke key) {
  VolumeControl *vc = ((VolumeControlInjector *)ii)->vc;
  if (KeystrokeCmp(key, vc->vol_down) && (vc->cur_vol < VOL_MIN)) {
    vc->cur_vol += 1;
  } else if (KeystrokeCmp(key, vc->vol_up) && (vc->cur_vol > VOL_MAX)) {
    vc->cur_vol -= 1;
  } else {
    return;
  }

#if DISPLAY_VOLUME_ADJUSTMENTS
  vc->lastTouch = clock_time_us();
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
  ac_change_volume(vc->ac, AUDIO_STREAM_MUSIC, vc->cur_vol);
}

void _volume_update(VolumeControl *vc) {
#if DISPLAY_VOLUME_ADJUSTMENTS
  Time elapsed = clock_time_us() - vc->lastTouch;
  r_bool display_should_be_visible =
      (elapsed > 0 && elapsed < VOLUME_DISPLAY_PERSISTENCE);
  if (vc->visible && !display_should_be_visible) {
    board_buffer_pop(&vc->bbuf);
    vc->visible = false;
  } else if (!vc->visible && display_should_be_visible) {
    board_buffer_push(&vc->bbuf, vc->boardnum);
    vc->visible = true;
  }
  if (vc->visible) {
    char str[8];
    str[0] = 'v';
    str[1] = 'o';
    str[2] = 'l';
    str[3] = 'u';
    str[4] = 'm';
    str[5] = 'e';
    str[6] = ' ';
    str[7] = '0' + (VOL_MIN - vc->cur_vol);
    ascii_to_bitmap_str(vc->bbuf.buffer, 8, str);
    board_buffer_draw(&vc->bbuf);
  }
  schedule_us(100000, (ActivationFuncPtr)_volume_update, vc);
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
}
