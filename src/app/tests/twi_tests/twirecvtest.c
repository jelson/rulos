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
#include "periph/7seg_panel/7seg_panel.h"

#define HAVE_AUDIOBOARD_UART 0

#if !HAVE_AUDIOBOARD_UART
#define SYNCDEBUG() \
  {}
#else

#define SYNCDEBUG() syncdebug(0, 'T', __LINE__)

#define FOSC 8000000  // Clock Speed
#define BAUD 38400
#define MYUBRR FOSC / 16 / BAUD - 1

void serial_init() { uart_init(RULOS_UART0, MYUBRR, TRUE); }

void waitbusyuart() {
  while (uart_busy(RULOS_UART0)) {
  }
}

void syncdebug(uint8_t spaces, char f, uint16_t line) {
  char buf[16], hexbuf[6];
  int i;
  for (i = 0; i < spaces; i++) {
    buf[i] = ' ';
  }
  buf[i++] = f;
  buf[i++] = ':';
  buf[i++] = '\0';
  if (f >= 'a')  // lowercase -> hex value; so sue me
  {
    debug_itoha(hexbuf, line);
  } else {
    itoda(hexbuf, line);
  }
  strcat(buf, hexbuf);
  strcat(buf, "\n");
  uart_send(RULOS_UART0, (char *)buf, strlen(buf), NULL, NULL);
  waitbusyuart();
}
#endif  // HAVE_AUDIOBOARD_UART

void board_say(const char *s) {
  SSBitmap bm[8];
  int i;

  ascii_to_bitmap_str(bm, 8, s);

  for (i = 0; i < 8; i++) {
    display_controller_program_board(i, bm);
  }
}

static void recv_func(MessageRecvBuffer *msg) {
  const int payload_len = msg->payload_len;
  char buf[payload_len + 1];
  memcpy(buf, msg->data, payload_len);
  buf[payload_len] = '\0';
  net_free_received_message_buffer(msg);
  LOG("Got network message [%d bytes]: '%s'", payload_len, buf);
  SYNCDEBUG();

  // display the first 8 chars to the leds
  buf[min(payload_len, 8)] = '\0';
  board_say(buf);
}

void test_netstack() {
  Network net;
  const int MESSAGE_SIZE = 20;
  const int RING_SIZE = 4;
  unsigned char data[RECEIVE_RING_SIZE(RING_SIZE, MESSAGE_SIZE)];
  AppReceiver recv;

  board_say(" rEAdy  ");

  init_clock(10000, TIMER1);

  recv.recv_complete_func = recv_func;
  recv.port = AUDIO_PORT;
  recv.num_receive_buffers = RING_SIZE;
  recv.payload_capacity = MESSAGE_SIZE;
  recv.message_recv_buffers = data;

  SYNCDEBUG();
  init_twi_network(&net, 100, AUDIO_ADDR);
  net_bind_receiver(&net, &recv);
  SYNCDEBUG();

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();
}

int main() {
  hal_init();
  hal_init_rocketpanel();
#if HAVE_AUDIOBOARD_UART
  serial_init();
#endif

  test_netstack();
  return 0;
}
