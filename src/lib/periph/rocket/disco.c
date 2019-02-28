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
