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

#include "periph/rocket/screenblanker.h"

//////////////////////////////////////////////////////////////////////////////
// disco mode defs

#define _packed(c0, c1, c2, c3, c4, c5, c6, c7, zero)              \
  (0 | (((uint32_t)c0) << (7 * 4)) | (((uint32_t)c1) << (6 * 4)) | \
   (((uint32_t)c2) << (5 * 4)) | (((uint32_t)c3) << (4 * 4)) |     \
   (((uint32_t)c4) << (3 * 4)) | (((uint32_t)c5) << (2 * 4)) |     \
   (((uint32_t)c6) << (1 * 4)) | (((uint32_t)c7) << (0 * 4)))

#undef PR

#define PG DISCO_GREEN,
#define PR DISCO_RED,
#define PY DISCO_YELLOW,
#define PB DISCO_BLUE,

#define DBOARD(name, colors, x, y, remote_addr, remote_idx) _packed(colors 0)
#define B_NO_BOARD                                          /**/
#define B_END                                               /**/
#include "periph/7seg_panel/display_tree.ch"
static const uint32_t rocket_tree[] = {ROCKET_TREE};

//////////////////////////////////////////////////////////////////////////////

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker,
                                         UIEvent evt);
void screenblanker_update(ScreenBlanker *sb);
void screenblanker_update_once(ScreenBlanker *sb);

void init_screenblanker(ScreenBlanker *screenblanker, HPAM *hpam,
                        IdleAct *idle) {
  screenblanker->func = (UIEventHandlerFunc)screenblanker_handler;
  screenblanker->num_buffers = NUM_TOTAL_BOARDS;
  screenblanker->tree = rocket_tree;
  screenblanker->disco_color = DISCO_RED;
  screenblanker->hpam = hpam;

  uint8_t i;
  for (i = 0; i < screenblanker->num_buffers; i++) {
    // hpam_max_alpha keeps us, while doodling with our alphas,
    // from ever overwriting the hpam's special segments.
    if (hpam != NULL && i == hpam->bbuf.board_index) {
      screenblanker->hpam_max_alpha[i] = ~hpam->bbuf.alpha;
    } else {
      screenblanker->hpam_max_alpha[i] = 0xff;
    }

    board_buffer_init(
        &screenblanker->buffer[i] DBG_BBUF_LABEL("screenblanker"));
  }

  screenblanker->mode = sb_inactive;

  screenblanker->screenblanker_sender = NULL;

  schedule_us(1, (ActivationFuncPtr)screenblanker_update, screenblanker);

  if (idle != NULL) {
    idle_add_handler(idle, (UIEventHandler *)screenblanker);
  }
}

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker,
                                         UIEvent evt) {
  if (evt == evt_idle_nowidle) {
    screenblanker_setmode(screenblanker, sb_blankdots);
  } else if (evt == evt_idle_nowactive) {
    // system is active, so screenblanker is inactive.
    screenblanker_setmode(screenblanker, sb_inactive);
  }
  return uied_accepted;
}

void screenblanker_setmode(ScreenBlanker *screenblanker,
                           ScreenBlankerMode newmode) {
  uint8_t i;

  if (newmode == screenblanker->mode) {
    return;
  } else if (newmode == sb_inactive) {
    for (i = 0; i < screenblanker->num_buffers; i++) {
      if (board_buffer_is_stacked(&screenblanker->buffer[i])) {
        board_buffer_pop(&screenblanker->buffer[i]);
      }
    }
  } else {
    for (i = 0; i < screenblanker->num_buffers; i++) {
      if (!board_buffer_is_stacked(&screenblanker->buffer[i])) {
        board_buffer_push(&screenblanker->buffer[i], i);
      }
    }
  }
  screenblanker->mode = newmode;

  if (screenblanker->screenblanker_sender != NULL) {
    sbs_send(screenblanker->screenblanker_sender, screenblanker);
  }

  screenblanker_update_once(screenblanker);
}

void screenblanker_setdisco(ScreenBlanker *screenblanker,
                            DiscoColor disco_color) {
  screenblanker->disco_color = disco_color;

  if (screenblanker->screenblanker_sender != NULL) {
    sbs_send(screenblanker->screenblanker_sender, screenblanker);
  }

  screenblanker_update_once(screenblanker);
}

void screenblanker_update(ScreenBlanker *sb) {
  Time interval = 100000;  // update 0.1s when not doing animation
  if (sb->mode == sb_flicker || sb->mode == sb_flicker) {
    interval = 40000;  // update 0.01s when animating
  }
  schedule_us(interval, (ActivationFuncPtr)screenblanker_update, sb);

  screenblanker_update_once(sb);
}

void screenblanker_update_once(ScreenBlanker *sb) {
  uint8_t i, j;

  // always exclude hpam digits, na mattar whaat
  for (i = 0; i < sb->num_buffers; i++) {
    board_buffer_set_alpha(&sb->buffer[i], sb->hpam_max_alpha[i]);
  }

  switch (sb->mode) {
    case sb_inactive: {
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER, TRUE);
      }
      break;
    }
    case sb_blankdots: {
      for (i = 0; i < sb->num_buffers; i++) {
        for (j = 0; j < NUM_DIGITS; j++) {
          sb->buffer[i].buffer[j] = SSB_DECIMAL;
        }
      }
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER, TRUE);
      }
      break;
    }
    case sb_black: {
      for (i = 0; i < sb->num_buffers; i++) {
        for (j = 0; j < NUM_DIGITS; j++) {
          sb->buffer[i].buffer[j] = 0;
        }
      }
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER, TRUE);
      }
      break;
    }
    case sb_disco: {
      for (i = 0; i < sb->num_buffers; i++) {
        for (j = 0; j < NUM_DIGITS; j++) {
          sb->buffer[i].buffer[j] =
              ((sb->tree[i] >> (4 * (NUM_DIGITS - 1 - j))) & 0x0f) ==
                      (uint8_t)sb->disco_color
                  ? 0xff
                  : 0;
        }
      }
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER,
                      sb->disco_color == DISCO_WHITE);
      }
      break;
    }
    case sb_flicker: {
      SSBitmap color = (clock_time_us() % 79) < 39 ? 0xff : 0;
      color = 0;
      for (i = 0; i < sb->num_buffers; i++) {
        uint8_t alpha =
            // deadbeef_rand() &
            deadbeef_rand() & sb->hpam_max_alpha[i];
        for (j = 0; j < NUM_DIGITS; j++) {
          sb->buffer[i].buffer[j] = color;
        }
        board_buffer_set_alpha(&sb->buffer[i], alpha);
        // NB 'set_alpha includes a redraw.
      }
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER,
                      (deadbeef_rand() & 3) != 0);
      }
      break;
    }
    case sb_borrowed: {
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, HPAM_LIGHTING_FLICKER, TRUE);
      }
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void sbl_recv_func(MessageRecvBuffer *msg) {
  AppReceiver *const app_receiver = msg->app_receiver;
  ScreenBlankerListener *const sbl =
      (ScreenBlankerListener *)app_receiver->user_data;
  ScreenblankerPayload *const sp = (ScreenblankerPayload *)msg->data;
  assert(msg->payload_len == sizeof(ScreenblankerPayload));
  screenblanker_setdisco(&sbl->screenblanker, sp->disco_color);
  screenblanker_setmode(&sbl->screenblanker, sp->mode);
  net_free_received_message_buffer(msg);
  // LOG("sbl_recv_func got bits %x %x!", sp->mode, sp->disco_color);
}

void init_screenblanker_listener(ScreenBlankerListener *sbl, Network *network) {
  init_screenblanker(&sbl->screenblanker, NULL, NULL);
  sbl->app_receiver.recv_complete_func = sbl_recv_func;
  sbl->app_receiver.port = SCREENBLANKER_PORT;
  sbl->app_receiver.num_receive_buffers = 1;
  sbl->app_receiver.payload_capacity = sizeof(ScreenblankerPayload);
  sbl->app_receiver.user_data = sbl;
  sbl->app_receiver.message_recv_buffers = sbl->app_receiver_storage;

  net_bind_receiver(network, &sbl->app_receiver);
}

//////////////////////////////////////////////////////////////////////////////

void sb_message_sent(SendSlot *sendSlot);

void init_screenblanker_sender(ScreenBlankerSender *sbs, Network *network) {
  sbs->network = network;

  sbs->sendSlot.func = NULL;
  sbs->sendSlot.wire_msg = (WireMessage *)sbs->send_slot_storage;
  sbs->sendSlot.dest_addr = ROCKET1_ADDR;
  sbs->sendSlot.wire_msg->dest_port = SCREENBLANKER_PORT;
  sbs->sendSlot.payload_len = sizeof(ScreenblankerPayload);
  sbs->sendSlot.sending = FALSE;
}

void sbs_send(ScreenBlankerSender *sbs, ScreenBlanker *sb) {
  if (sbs->sendSlot.sending) {
    return;
  }

  ScreenblankerPayload *sp =
      (ScreenblankerPayload *)sbs->sendSlot.wire_msg->data;
  sp->mode = sb->mode;
  sp->disco_color = sb->disco_color;
  net_send_message(sbs->network, &sbs->sendSlot);
}
