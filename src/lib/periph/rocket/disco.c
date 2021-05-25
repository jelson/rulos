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

#include "periph/rocket/disco.h"
#include "periph/audio/sound.h"

void disco_update(Disco *disco);
void disco_paint_once(Disco *disco);
UIEventDisposition disco_event_handler(UIEventHandler *raw_handler,
                                       UIEvent evt);
static void disco_recv_metadata(MessageRecvBuffer *msg);

#define SCROLL_METADATA_BOARD_NUM (1)

void disco_init(Disco *disco, AudioClient *audioClient,
                ScreenBlanker *screenblanker, IdleAct *idle, Network* network) {
  disco->handler.uieh.func = (UIEventHandlerFunc)disco_event_handler;
  disco->handler.disco = disco;

  disco->audioClient = audioClient;
  disco->screenblanker = screenblanker;
  disco->focused = FALSE;

  dscrlmsg_init(&disco->scroll_metadata, SCROLL_METADATA_BOARD_NUM, "", 120);
  board_buffer_pop(&disco->scroll_metadata.bbuf); // Hide for now

  disco->app_receiver.recv_complete_func = disco_recv_metadata;
  disco->app_receiver.port = MUSIC_METADATA_PORT;
  disco->app_receiver.num_receive_buffers = 1;
  disco->app_receiver.payload_capacity = sizeof(MusicMetadataMessage);
  disco->app_receiver.message_recv_buffers = disco->recv_ring_alloc;
  disco->app_receiver.user_data = disco;
  net_bind_receiver(network, &disco->app_receiver);

  idle_add_handler(idle, &disco->handler.uieh);

  schedule_us(1, (ActivationFuncPtr)disco_update, disco);
}

void disco_recv_metadata(MessageRecvBuffer *msg) {
  Disco *disco = (Disco *)msg->app_receiver->user_data;
  MusicMetadataMessage *mmm = (MusicMetadataMessage *)msg->data;
  assert(msg->payload_len == sizeof(MusicMetadataMessage));

  //memcpy(&disco->music_metadata, mmm, sizeof(MusicMetadataMessage));
  char* display = disco->music_metadata.path;
  strcpy(display, mmm->path+7); // skip "/music/"
  if (strlen(display)>4) {
    strcpy(&display[strlen(display)-4], "  ");  // Cover ".raw"
  }
  
  LOG("Music playing: %s", display);
  dscrlmsg_set_msg(&disco->scroll_metadata, display);
  net_free_received_message_buffer(msg);
}

void disco_update(Disco *disco) {
  schedule_us(100000, (ActivationFuncPtr)disco_update, disco);
  disco_paint_once(disco);
}

static void change_music(Disco *disco, int increment) {
  ac_send_music_control(disco->audioClient, increment);
}

static void disco_set_focus(Disco *disco, bool focused) {
  screenblanker_setmode(disco->screenblanker, focused ? sb_disco : sb_inactive);
  if (!disco->focused && focused) {
    board_buffer_push(&disco->scroll_metadata.bbuf, SCROLL_METADATA_BOARD_NUM);
    change_music(disco, +1);
  }
  if (disco->focused && !focused) {
    if (board_buffer_is_stacked(&disco->scroll_metadata.bbuf)) {
      board_buffer_pop(&disco->scroll_metadata.bbuf);
    }
    // remove focus
    ac_skip_to_clip(disco->audioClient, AUDIO_STREAM_MUSIC, sound_silence,
                    sound_silence);
  }
  disco->focused = focused;
}

void disco_paint_once(Disco *disco) {
  if (disco->focused) {
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
      disco_set_focus(disco, TRUE);
      break;
    case uie_escape:
      disco_set_focus(disco, FALSE);
      result = uied_blur;
      break;
    case evt_idle_nowidle:
      // Just silence, but don't remove focus
      ac_skip_to_clip(disco->audioClient, AUDIO_STREAM_MUSIC, sound_silence,
                      sound_silence);
      break;
    case 'a':
    case 'e':
      change_music(disco, +1);
      break;
    case 'b':
    case 'f':
      change_music(disco, -1);
      break;
  }
  return result;
}
