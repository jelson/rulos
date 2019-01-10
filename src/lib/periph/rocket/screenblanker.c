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
#define B_NO_BOARD /**/
#define B_END      /**/

#include "core/board_defs.h"

#if defined(BOARDCONFIG_ROCKET0) || defined(BOARDCONFIG_NETROCKET)
static uint32_t rocket_tree[] = {T_ROCKET0};
#elif defined(BOARDCONFIG_ROCKET1)
static uint32_t rocket_tree[] = {T_ROCKET1};
#else
#error "Screenblanker is confused"
#endif

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
        hpam_set_port(sb->hpam, hpam_lighting_flicker, TRUE);
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
        hpam_set_port(sb->hpam, hpam_lighting_flicker, TRUE);
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
        hpam_set_port(sb->hpam, hpam_lighting_flicker, TRUE);
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
        hpam_set_port(sb->hpam, hpam_lighting_flicker,
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
        hpam_set_port(sb->hpam, hpam_lighting_flicker,
                      (deadbeef_rand() & 3) != 0);
      }
      break;
    }
    case sb_borrowed: {
      if (sb->hpam != NULL) {
        hpam_set_port(sb->hpam, hpam_lighting_flicker, TRUE);
      }
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void sbl_recv_func(RecvSlot *recvSlot, uint8_t payload_len);

void init_screenblanker_listener(ScreenBlankerListener *sbl, Network *network) {
  init_screenblanker(&sbl->screenblanker, NULL, NULL);
  sbl->recvSlot.func = sbl_recv_func;
  sbl->recvSlot.port = SCREENBLANKER_PORT;
  sbl->recvSlot.payload_capacity = sizeof(ScreenblankerPayload);
  sbl->recvSlot.msg_occupied = FALSE;
  sbl->recvSlot.msg = (Message *)sbl->message_storage;
  sbl->recvSlot.user_data = sbl;

  net_bind_receiver(network, &sbl->recvSlot);
}

void sbl_recv_func(RecvSlot *recvSlot, uint8_t payload_len) {
  ScreenBlankerListener *sbl = (ScreenBlankerListener *)recvSlot->user_data;
  ScreenblankerPayload *sp = (ScreenblankerPayload *)sbl->recvSlot.msg->data;
  assert(payload_len == sizeof(ScreenblankerPayload));
  screenblanker_setdisco(&sbl->screenblanker, sp->disco_color);
  screenblanker_setmode(&sbl->screenblanker, sp->mode);
  sbl->recvSlot.msg_occupied = FALSE;
  // LOG("sbl_recv_func got bits %x %x!\n", sp->mode, sp->disco_color);
}

//////////////////////////////////////////////////////////////////////////////

void sb_message_sent(SendSlot *sendSlot);

void init_screenblanker_sender(ScreenBlankerSender *sbs, Network *network) {
  sbs->network = network;

  sbs->sendSlot.func = NULL;
  sbs->sendSlot.msg = (Message *)sbs->message_storage;
  sbs->sendSlot.dest_addr = ROCKET1_ADDR;
  sbs->sendSlot.msg->dest_port = SCREENBLANKER_PORT;
  sbs->sendSlot.msg->payload_len = sizeof(ScreenblankerPayload);
  sbs->sendSlot.sending = FALSE;
}

void sbs_send(ScreenBlankerSender *sbs, ScreenBlanker *sb) {
  if (sbs->sendSlot.sending) return;

  ScreenblankerPayload *sp = (ScreenblankerPayload *)sbs->sendSlot.msg->data;
  sp->mode = sb->mode;
  sp->disco_color = sb->disco_color;
  net_send_message(sbs->network, &sbs->sendSlot);
}
