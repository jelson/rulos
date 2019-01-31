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

#include "periph/rocket/disco.h"

#include "periph/rocket/sound.h"

void disco_update(Disco *disco);
void disco_paint_once(Disco *disco);
UIEventDisposition disco_event_handler(UIEventHandler *raw_handler,
                                       UIEvent evt);

void disco_init(Disco *disco, AudioClient *audioClient,
                ScreenBlanker *screenblanker, IdleAct *idle) {
  disco->handler.uieh.func = (UIEventHandlerFunc)disco_event_handler;
  disco->handler.disco = disco;

  disco->audioClient = audioClient;
  disco->screenblanker = screenblanker;
  disco->focused = FALSE;

  idle_add_handler(idle, &disco->handler.uieh);

  schedule_us(1, (ActivationFuncPtr)disco_update, disco);
}

void disco_update(Disco *disco) {
  schedule_us(100000, (ActivationFuncPtr)disco_update, disco);
  disco_paint_once(disco);
}

void disco_paint_once(Disco *disco) {
  if (disco->focused) {
    screenblanker_setmode(disco->screenblanker, sb_disco);
    uint8_t disco_color = deadbeef_rand() % 6;
    // LOG("disco color %x", disco_color);
    screenblanker_setdisco(disco->screenblanker, (DiscoColor)disco_color);
  }
}

UIEventDisposition disco_event_handler(UIEventHandler *raw_handler,
                                       UIEvent evt) {
  Disco *disco = ((DiscoHandler *)raw_handler)->disco;

  UIEventDisposition result = uied_accepted;
  switch (evt) {
    case uie_focus:
      ac_send_music_control(disco->audioClient, +1);
      disco->focused = TRUE;
      break;
    case uie_escape:
      disco->focused = FALSE;
      screenblanker_setmode(disco->screenblanker, sb_inactive);
      result = uied_blur;
      ac_skip_to_clip(disco->audioClient, AUDIO_STREAM_MUSIC, sound_silence,
                      sound_silence);
      break;
    case evt_idle_nowidle:
      ac_skip_to_clip(disco->audioClient, AUDIO_STREAM_MUSIC, sound_silence,
                      sound_silence);
      break;
    case 'a':
    case 'e':
      ac_send_music_control(disco->audioClient, +1);
      break;
    case 'b':
    case 'f':
      ac_send_music_control(disco->audioClient, -1);
      break;
  }
  return result;
}
