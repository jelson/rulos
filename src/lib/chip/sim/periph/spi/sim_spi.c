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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

/**************** spi *********************/

typedef enum {
  sss_ready,
  sss_read_addr,
  sss_read_fetch,
} SimSpiState;

typedef struct s_sim_spi {
  bool initted;
  HALSPIHandler *spi_handler;
  FILE *fp;
  SimSpiState state;
  int addr_off;
  uint8_t addr[3];
  int16_t result;
  uint8_t recursions;
} SimSpi;
SimSpi g_spi = {FALSE};

static void sim_spi_poll(void *data);

void hal_init_spi() {
  g_spi.spi_handler = NULL;
  g_spi.fp = fopen("obj.sim/spiflash.bin", "r");
  assert(g_spi.fp != NULL);
  g_spi.initted = TRUE;
  sim_register_clock_handler(sim_spi_poll, NULL);
}

void hal_spi_set_fast(bool fast) {}

void hal_spi_select_slave(bool select) {
  if (select) {
    g_spi.state = sss_ready;
  }
}

void hal_spi_set_handler(HALSPIHandler *handler) {
  g_spi.spi_handler = handler;
}

void hal_spi_send(uint8_t byte) {
  g_spi.recursions += 1;

  uint8_t result = 0;
  switch (g_spi.state) {
    case sss_ready:
      LOG("sim:spi cmd %x", byte);
      if (byte == SPIFLASH_CMD_READ_DATA) {
        g_spi.state = sss_read_addr;
        g_spi.addr_off = 0;
      } else {
        assert(FALSE);  // "unsupported SPI command"
      }
      break;
    case sss_read_addr:
      // LOG("sim:spi addr[%d] == %x", g_spi.addr_off, byte);
      g_spi.addr[g_spi.addr_off++] = byte;
      if (g_spi.addr_off == 3) {
        int addr = (((int)g_spi.addr[0]) << 16) | (((int)g_spi.addr[1]) << 8) |
                   (((int)g_spi.addr[2]) << 0);
        LOG("sim:spi addr == %x", addr);
        fseek(g_spi.fp, addr, SEEK_SET);
        g_spi.state = sss_read_fetch;
      }
      break;
    case sss_read_fetch:
      result = fgetc(g_spi.fp);
      // LOG("sim:spi read fetch == %x", result);
      break;
  }

  // deliver on stack. This may be a little infinite-recursion-y
  if (g_spi.recursions > 10) {
    g_spi.result = result;
  } else {
    g_spi.spi_handler->func(g_spi.spi_handler, result);
  }
  g_spi.recursions -= 1;
}

static void sim_spi_poll(void *data) {
  if (!g_spi.initted) {
    return;
  }

  // LOG("sim_spi_poke delivering deferred result");
  if (g_spi.result >= 0) {
    uint8_t result = g_spi.result;
    g_spi.result = -1;
    g_spi.spi_handler->func(g_spi.spi_handler, result);
  }
}

void hal_spi_close() { g_spi.state = sss_ready; }
