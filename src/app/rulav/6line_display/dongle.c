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

#include "core/rulos.h"
#include "core/twi.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/7seg_panel/remote_bbuf.h"
#include "core/bss_canary.h"
#include "periph/rocket6line/rocket6line.h"

// For legacy backcompat with the old 4-line matrix, we assume the first line of
// the matrix is index 3.
#define FIRST_LINE_IDX 3

typedef struct {
  Network net;
  RemoteBBufRecv rbr;
#ifdef LOG_TO_SERIAL
  HalUart uart;
#endif
} Rocket6LineDongle_t;

Rocket6LineDongle_t r6l_dongle;

static void r6l_bbuf_recv(MessageRecvBuffer *msg) {
  if (msg->payload_len != sizeof(BBufMessage)) {
    LOG("rbr_recv: Error: expected BBufMessage of size %zu, got %d bytes",
        sizeof(BBufMessage), msg->payload_len);
    goto done;
  }

  BBufMessage *bbm = (BBufMessage *)msg->data;

  if (bbm->index < FIRST_LINE_IDX ||
      bbm->index >= FIRST_LINE_IDX + ROCKET6LINE_NUM_ROWS) {
    LOG("got unexpected row index %d", bbm->index);
    goto done;
  }

  rocket6line_write_line(bbm->index - FIRST_LINE_IDX, bbm->buf, NUM_DIGITS);

done:
  net_free_received_message_buffer(msg);
}

static void init_r6l_bbuf_recv(RemoteBBufRecv *rbr, Network *network) {
  rbr->app_receiver.recv_complete_func = r6l_bbuf_recv,
  rbr->app_receiver.port = REMOTE_BBUF_PORT;
  rbr->app_receiver.num_receive_buffers = REMOTE_BBUF_RING_SIZE;
  rbr->app_receiver.payload_capacity = sizeof(BBufMessage);
  rbr->app_receiver.message_recv_buffers = rbr->recv_ring_alloc;

  net_bind_receiver(network, &rbr->app_receiver);
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&r6l_dongle.uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(10000, TIMER1);
  bss_canary_init();
  rocket6line_init();
  init_twi_network(&r6l_dongle.net, 100, DONGLE_BASE_ADDR + DONGLE_BOARD_ID);
  init_r6l_bbuf_recv(&r6l_dongle.rbr, &r6l_dongle.net);

  cpumon_main_loop();
}
